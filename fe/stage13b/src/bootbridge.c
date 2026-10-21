/* RTX 3080 FE Stage 13: GPU-READ-ONLY UEFI -> existing Windows Boot Manager.
 * Does not run the GPU unlock, change UEFI variables or disconnect a driver.
 * All UEFI calls are through the firmware's provided tables (no CRT imports).
 */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Guid/GlobalVariable.h>

#define MAX_BOOT_ORDER 128U
#define MAX_LOAD_OPTION 32768U
#define MAX_GOP 32U
#define MAX_PCI 256U
static EFI_GUID LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID FsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID GlobalGuid=EFI_GLOBAL_VARIABLE;
static EFI_SYSTEM_TABLE *ST;
static EFI_FILE_PROTOCOL *LogFile;

static UINTN textlen(const CHAR8 *s){ UINTN i=0;while(s[i]&&i<2048)i++;return i; }
static void out(const CHAR8 *s){
  if(LogFile){ UINTN n=textlen(s);(void)LogFile->Write(LogFile,&n,(VOID*)s); }
  if(ST&&ST->ConOut&&ST->ConOut->OutputString){
    CHAR16 b[240]; UINTN j=0;
    for(UINTN i=0;s[i]&&i<190;i++){
      if(s[i]=='\n'){b[j++]=L'\r';b[j++]=L'\n';}else b[j++]=(UINT8)s[i];
    } b[j]=0; (void)ST->ConOut->OutputString(ST->ConOut,b);
  }
}
static void hex(UINT64 v,UINTN digits){
  CHAR8 b[20]; if(digits>16)digits=16;
  for(UINTN i=0;i<digits;i++){UINTN n=(UINTN)((v>>((digits-1-i)*4))&15U);b[i]=(CHAR8)(n<10?'0'+n:'a'+n-10);} b[digits]=0;out(b);
}
static void number(const CHAR8 *label,UINT64 v){out(label);out("0x");hex(v,16);out("\n");}
static void status(const CHAR8 *label,EFI_STATUS v){number(label,(UINT64)v);}
static CHAR16 low(CHAR16 c){return(c>=L'A'&&c<=L'Z')?(CHAR16)(c+32):c;}
static BOOLEAN path_is_windows(const UINT8 *bytes,UINTN n){
  /* EFI_LOAD_OPTION file path list: well-formed nodes with a Microsoft bootmgfw.efi filepath node. */
  static const CHAR16 want[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
  UINTN i=0;BOOLEAN seen=FALSE,terminal=FALSE;
  while(i+4<=n){
    UINT8 type=(UINT8)(bytes[i]&0x7fU),sub=bytes[i+1];
    UINTN len=(UINTN)bytes[i+2]|((UINTN)bytes[i+3]<<8);
    if(len<4||len>n-i)return FALSE;
    if(type==0x7fU&&sub==0xffU){if(len!=4)return FALSE;terminal=TRUE;return terminal&&seen&&i+len==n;}
    if(type==0x04U&&sub==0x04U){
      UINTN cnt=(len-4)/2;
      if((len-4)%2)return FALSE;
      UINTN wl=0;while(want[wl])wl++;
      if(cnt>=wl+1){
        /* Search the UTF-16LE node, exactly matching the Windows file path. */
        BOOLEAN match=TRUE;
        for(UINTN j=0;j<wl;j++){
          CHAR16 c=(CHAR16)((UINT16)bytes[i+4+j*2]|((UINT16)bytes[i+5+j*2]<<8));
          if(low(c)!=low(want[j])){match=FALSE;break;}
        }
        if(match&&bytes[i+4+wl*2]==0&&bytes[i+5+wl*2]==0)seen=TRUE;
      }
    }
    i+=len;
  }
  return FALSE;
}
static EFI_STATUS var(const CHAR16 *name,VOID **data,UINTN *size){
  UINTN n=0;EFI_STATUS rc=ST->RuntimeServices->GetVariable((CHAR16*)name,&GlobalGuid,NULL,&n,NULL);
  if(rc!=EFI_BUFFER_TOO_SMALL||n==0||n>MAX_LOAD_OPTION)return EFI_UNSUPPORTED;
  VOID *buf=NULL;rc=ST->BootServices->AllocatePool(EfiLoaderData,n,&buf);
  if(EFI_ERROR(rc))return rc;
  rc=ST->RuntimeServices->GetVariable((CHAR16*)name,&GlobalGuid,NULL,&n,buf);
  if(EFI_ERROR(rc)){ST->BootServices->FreePool(buf);return rc;}
  *data=buf;*size=n;return EFI_SUCCESS;
}
static BOOLEAN find_bootmgr(VOID **option,UINTN *optlen,UINT16 *bootid,UINTN *dp_offset){
  VOID *order=NULL;UINTN osz=0; EFI_STATUS rc=var(L"BootOrder",&order,&osz);
  status("bootorder_status=",rc);
  if(EFI_ERROR(rc)||osz%2||osz/2>MAX_BOOT_ORDER){if(order)ST->BootServices->FreePool(order);return FALSE;}
  for(UINTN i=0;i<osz/2;i++){
    UINT8 *ids=(UINT8*)order;
    UINT16 id=(UINT16)((UINT16)ids[i*2]|((UINT16)ids[i*2+1]<<8));
    CHAR16 name[9]=L"Boot0000";
    for(UINTN j=0;j<4;j++){UINTN digit=(id>>((3-j)*4))&15;name[4+j]=(CHAR16)(digit<10?L'0'+digit:L'A'+digit-10);}
    VOID *buff=NULL;UINTN size=0;rc=var(name,&buff,&size);
    if(EFI_ERROR(rc))continue;
    UINT8 *b=(UINT8*)buff;
    if(size<12){ST->BootServices->FreePool(buff);continue;}
    UINT32 attr=(UINT32)b[0]|((UINT32)b[1]<<8)|((UINT32)b[2]<<16)|((UINT32)b[3]<<24);
    UINTN dplen=(UINTN)b[4]|((UINTN)b[5]<<8);
    UINTN p=6;BOOLEAN nullterm=FALSE;
    while(p+1<size){UINT16 c=(UINT16)b[p]|((UINT16)b[p+1]<<8);p+=2;if(c==0){nullterm=TRUE;break;}}
    if((attr&1U)&&nullterm&&dplen>=4&&dplen<=size-p&&path_is_windows(b+p,dplen)){
      out("windows_boot_option=Boot");hex(id,4);out("\n");
      *option=buff;*optlen=size;*bootid=id;*dp_offset=p;
      ST->BootServices->FreePool(order);return TRUE;
    }
    ST->BootServices->FreePool(buff);
  }
  ST->BootServices->FreePool(order);
  out("windows_boot_option=NOT_FOUND\n");return FALSE;
}
/* EFI Boot#### commonly stores an abbreviated HD(...)/File(...) path.
 * Boot Manager expands this path; bare LoadImage does not have to do so.
 * Fallback: match the exact HD node with a mounted EFI filesystem and
 * construct its complete controller/partition/File device path.
 * Never guess another partition and never modify NVRAM or filesystem data.
 */
static BOOLEAN path_hd_node(const UINT8 *p,UINTN max,const UINT8 **hd,UINTN *hdlen,UINTN *end){
  if(!p||max<4||!hd||!hdlen||!end)return FALSE;
  *hd=NULL;*hdlen=0;*end=0;
  UINTN pos=0;
  for(UINTN step=0;step<48 && pos+4<=max;step++){
    UINT8 type=p[pos]&0x7fU,sub=p[pos+1];
    UINTN len=(UINTN)p[pos+2]|((UINTN)p[pos+3]<<8);
    if(len<4||len>max-pos)return FALSE;
    if(type==0x7fU){
      if(sub!=0xffU||len!=4)return FALSE;
      *end=pos;return TRUE;
    }
    if(type==0x04U&&sub==0x01U){ /* MEDIA_HARDDRIVE_DP */
      if(len!=42U||*hd)return FALSE;
      *hd=p+pos;*hdlen=len;
    }
    pos+=len;
  }
  return FALSE;
}
static BOOLEAN bytes_equal(const UINT8 *a,const UINT8 *b,UINTN n){
  for(UINTN j=0;j<n;j++)if(a[j]!=b[j])return FALSE;
  return TRUE;
}
static EFI_STATUS load_windows_from_matching_esp(EFI_HANDLE image,const UINT8 *option,
                                                  UINTN optionlen,UINTN dp_offset,
                                                  EFI_HANDLE *windows){
  static const CHAR16 win_path[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
  if(!option||!windows||optionlen<6||dp_offset>=optionlen)return EFI_INVALID_PARAMETER;
  UINTN dplen=(UINTN)option[4]|((UINTN)option[5]<<8);
  if(!dplen||dplen>optionlen-dp_offset)return EFI_COMPROMISED_DATA;
  const UINT8 *want_hd=NULL;UINTN want_hlen=0,want_end=0;
  if(!path_hd_node(option+dp_offset,dplen,&want_hd,&want_hlen,&want_end)||
     want_end+4!=dplen||!want_hd){
    out("fallback_boot_option_hd=ABSENT_OR_INVALID\n");return EFI_UNSUPPORTED;
  }
  out("fallback_boot_option_hd=FOUND\n");
  EFI_HANDLE *handles=NULL;UINTN count=0;
  EFI_STATUS rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&FsGuid,NULL,&count,&handles);
  status("fallback_fs_enum_status=",rc);
  if(EFI_ERROR(rc)||!handles||count==0||count>256){
    if(handles)ST->BootServices->FreePool(handles);
    return EFI_NOT_FOUND;
  }
  number("fallback_fs_handles=",count);
  EFI_STATUS result=EFI_NOT_FOUND;UINTN hd_matches=0,file_matches=0;
  for(UINTN i=0;i<count;i++){
    EFI_DEVICE_PATH_PROTOCOL *fullpath=NULL;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
    if(EFI_ERROR(ST->BootServices->HandleProtocol(handles[i],&PathGuid,(VOID**)&fullpath))||
       !fullpath||EFI_ERROR(ST->BootServices->HandleProtocol(handles[i],&FsGuid,(VOID**)&fs))||!fs)
      continue;
    const UINT8 *have_hd=NULL;UINTN have_hlen=0,prefix=0;
    if(!path_hd_node((const UINT8*)fullpath,1024,&have_hd,&have_hlen,&prefix)||
       !have_hd||have_hlen!=want_hlen||!bytes_equal(have_hd,want_hd,want_hlen))continue;
    hd_matches++;
    EFI_FILE_PROTOCOL *root=NULL,*file=NULL;
    rc=fs->OpenVolume(fs,&root);
    status("fallback_open_volume_status=",rc);
    if(EFI_ERROR(rc)||!root)continue;
    rc=root->Open(root,&file,(CHAR16*)win_path,EFI_FILE_MODE_READ,0);
    status("fallback_bootmgfw_open_status=",rc);
    if(file){(void)file->Close(file);file=NULL;}
    (void)root->Close(root);
    if(EFI_ERROR(rc))continue;
    file_matches++;
    UINTN filenode=4U+sizeof(win_path),total=prefix+filenode+4U;
    if(total>2048U||filenode>65535U){result=EFI_COMPROMISED_DATA;continue;}
    UINT8 *built=NULL;
    rc=ST->BootServices->AllocatePool(EfiLoaderData,total,(VOID**)&built);
    if(EFI_ERROR(rc)||!built){result=EFI_OUT_OF_RESOURCES;break;}
    for(UINTN j=0;j<prefix;j++)built[j]=((const UINT8*)fullpath)[j];
    built[prefix+0]=0x04;built[prefix+1]=0x04;
    built[prefix+2]=(UINT8)filenode;built[prefix+3]=(UINT8)(filenode>>8);
    for(UINTN j=0;j<sizeof(win_path);j++)built[prefix+4+j]=((const UINT8*)win_path)[j];
    built[prefix+filenode]=0x7f;built[prefix+filenode+1]=0xff;
    built[prefix+filenode+2]=4;built[prefix+filenode+3]=0;
    rc=ST->BootServices->LoadImage(FALSE,image,(EFI_DEVICE_PATH_PROTOCOL*)built,NULL,0,windows);
    status("fallback_complete_path_loadimage_status=",rc);
    ST->BootServices->FreePool(built);
    result=rc;
    if(!EFI_ERROR(rc)&&*windows)break;
  }
  number("fallback_matching_hd_handles=",hd_matches);
  number("fallback_existing_bootmgfw_files=",file_matches);
  if(handles)ST->BootServices->FreePool(handles);
  return result;
}
static BOOLEAN prefix_matches(const EFI_DEVICE_PATH_PROTOCOL *c,const EFI_DEVICE_PATH_PROTOCOL *g){
  if(!c||!g)return FALSE;
  const UINT8 *a=(const UINT8*)c,*b=(const UINT8*)g;
  UINTN i=0;
  for(UINTN nodes=0;nodes<24;nodes++){
    UINT8 t=(UINT8)(a[i]&0x7f),s=a[i+1]; UINTN len=(UINTN)a[i+2]|((UINTN)a[i+3]<<8);
    UINTN blen=(UINTN)b[i+2]|((UINTN)b[i+3]<<8);
    if(len<4||len>128||i+len>512)return FALSE;
    if(t==0x7f&&s==0xff)return TRUE;
    if(blen!=len||blen<4||blen>128||i+blen>512)return FALSE;
    if((b[i]&0x7f)==0x7f)return FALSE;
    for(UINTN j=0;j<len;j++)if(a[i+j]!=b[i+j])return FALSE;
    i+=len;
  }
  return FALSE;
}
static void inspect_owners(EFI_HANDLE h,EFI_GUID *guid,const CHAR8 *kind){
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;
  UINTN n=0;
  EFI_STATUS rc=ST->BootServices->OpenProtocolInformation(h,guid,&entries,&n);
  out(kind);out("_owner_query_status=0x");hex((UINT64)rc,16);out("\n");
  if(!EFI_ERROR(rc)){
    if(n>128){out("owner_count_abnormal\n");}
    else {
      out(kind);out("_owner_count=0x");hex(n,4);out("\n");
      for(UINTN j=0;j<n;j++){
        out(kind);out("_owner_index=0x");hex(j,4);
        out(" attribute=0x");hex(entries[j].Attributes,8);
        out(" open_count=0x");hex(entries[j].OpenCount,8);
        out("\n");
      }
    }
  }
  if(entries)ST->BootServices->FreePool(entries);
}
static void inspect_gpu(VOID){
  EFI_HANDLE *pcis=NULL;UINTN count=0,matched=0,amd=0;
  EFI_DEVICE_PATH_PROTOCOL *gpu_path=NULL;
  EFI_STATUS rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&count,&pcis);
  status("pci_enum_status=",rc);
  if(EFI_ERROR(rc)||!pcis||count>MAX_PCI){if(pcis)ST->BootServices->FreePool(pcis);return;}
  for(UINTN i=0;i<count;i++){
    EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 id=0,cls=0,cmd=0;
    rc=ST->BootServices->HandleProtocol(pcis[i],&PciGuid,(VOID**)&pci);
    if(EFI_ERROR(rc)||!pci)continue;
    if(EFI_ERROR(pci->Pci.Read(pci,EfiPciIoWidthUint32,0,1,&id)))continue;
    if(EFI_ERROR(pci->Pci.Read(pci,EfiPciIoWidthUint32,8,1,&cls)))continue;
    if(((cls>>24)&0xffU)!=3U)continue;
    if((id&0xffffU)==0x1002U)amd++;
    if(id==0x220610deU){
      matched++;
      UINTN seg=0,bus=0,dev=0,func=0;
      status("fe_location_status=",pci->GetLocation(pci,&seg,&bus,&dev,&func));
      number("fe_segment=",seg);number("fe_bus=",bus);number("fe_device=",dev);number("fe_function=",func);
      status("fe_cmd_read_status=",pci->Pci.Read(pci,EfiPciIoWidthUint32,4,1,&cmd));
      number("fe_pci_command=",cmd&0xffffU);
      UINT32 bar=0;status("fe_bar0_read_status=",pci->Pci.Read(pci,EfiPciIoWidthUint32,0x10,1,&bar));
      number("fe_bar0_raw=",bar);
      status("fe_device_path_status=",ST->BootServices->HandleProtocol(pcis[i],&PathGuid,(VOID**)&gpu_path));
      inspect_owners(pcis[i],&PciGuid,"fe_pci");
      /* Never disconnect or open this graphics driver EXCLUSIVE. */
    }
  }
  number("nvidia_fe_display_count=",matched);number("amd_display_count=",amd);
  ST->BootServices->FreePool(pcis);
  EFI_HANDLE *gops=NULL;UINTN ng=0,matched_gop=0;
  rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&ng,&gops);
  status("gop_enum_status=",rc);number("gop_count=",ng);
  if(EFI_ERROR(rc)||!gops||ng>MAX_GOP){if(gops)ST->BootServices->FreePool(gops);return;}
  for(UINTN i=0;i<ng;i++){
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;
    EFI_STATUS gp=ST->BootServices->HandleProtocol(gops[i],&GopGuid,(VOID**)&gop);
    EFI_STATUS dp=ST->BootServices->HandleProtocol(gops[i],&PathGuid,(VOID**)&path);
    BOOLEAN is_gpu=!EFI_ERROR(dp)&&prefix_matches(gpu_path,path);
    if(is_gpu){matched_gop++;inspect_owners(gops[i],&GopGuid,"fe_gop");}
    out("gop_index=0x");hex(i,4);out(" matched_fe=");hex(is_gpu,1);
    out(" is_console=");hex(gops[i]==ST->ConsoleOutHandle,1);out("\n");
    if(!EFI_ERROR(gp)&&gop&&gop->Mode){
      number("gop_framebuffer_base=",gop->Mode->FrameBufferBase);
      number("gop_framebuffer_bytes=",gop->Mode->FrameBufferSize);
      if(gop->Mode->Info && gop->Mode->SizeOfInfo>=sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)){
        number("gop_width=",gop->Mode->Info->HorizontalResolution);
        number("gop_height=",gop->Mode->Info->VerticalResolution);
      }
    }
  }
  ST->BootServices->FreePool(gops);number("nvidia_fe_gop_routes=",matched_gop);
  if(matched==1&&amd==0&&matched_gop>=1)out("sole_gpu_topology=OBSERVED\n");
  else out("sole_gpu_topology=NOT_CONFIRMED\n");
}
static void make_log_name(CHAR16 *name){
  static const CHAR16 prefix[]=L"\\RTX3080-BOOT-";
  UINTN i=0;while(prefix[i]){name[i]=prefix[i];i++;}
  EFI_TIME t={0};EFI_STATUS rc=ST->RuntimeServices->GetTime(&t,NULL);
  UINTN vals[6]={(UINTN)t.Year,(UINTN)t.Month,(UINTN)t.Day,(UINTN)t.Hour,(UINTN)t.Minute,(UINTN)t.Second};
  UINTN widths[6]={4,2,2,2,2,2};
  for(UINTN k=0;k<6;k++)for(UINTN x=0;x<widths[k];x++){
    UINTN power=1;for(UINTN z=x+1;z<widths[k];z++)power*=10;
    name[i++]=(CHAR16)(L'0'+(rc==EFI_SUCCESS?((vals[k]/power)%10):0));
  }
  static const CHAR16 ext[]=L".TXT";
  for(UINTN k=0;ext[k];k++)name[i++]=ext[k];name[i]=0;
}
static void open_log(EFI_HANDLE image){
  EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
  EFI_STATUS rc=ST->BootServices->HandleProtocol(image,&LoadedGuid,(VOID**)&loaded);
  if(EFI_ERROR(rc)||!loaded)return;
  rc=ST->BootServices->HandleProtocol(loaded->DeviceHandle,&FsGuid,(VOID**)&fs);
  if(EFI_ERROR(rc)||!fs)return;
  EFI_FILE_PROTOCOL *root=NULL;rc=fs->OpenVolume(fs,&root);
  if(EFI_ERROR(rc)||!root)return;
  CHAR16 name[70];make_log_name(name);
  rc=root->Open(root,&LogFile,name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  (void)root->Close(root);
  if(EFI_ERROR(rc))LogFile=NULL;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *st){
  if(!st||!st->BootServices||!st->RuntimeServices)return EFI_INVALID_PARAMETER;
  ST=st;open_log(image);
  out("RTX3080 FE stage13b: READ-ONLY GOP & BOOTBRIDGE EFI_NOT_FOUND fallback v2\n");
  out("NO GPU WRITES; NO MSE; NO DMA; NO FLR; NO UNLOCK; NO BOOT VARIABLE WRITES\n");
  inspect_gpu();
  VOID *option=NULL;UINTN length=0,offset=0;UINT16 id=0;
  BOOLEAN found=find_bootmgr(&option,&length,&id,&offset);
  out("windows_bootmgr_discovered=");hex(found,1);out("\n");
  if(!found){out("BOOTBRIDGE RESULT: Windows not found; return to firmware\n");goto finish;}
  out("BOOTBRIDGE: attempting Windows Boot Manager by firmware BootOrder (no NVRAM change)\n");
  if(LogFile)(void)LogFile->Flush(LogFile);
  EFI_HANDLE windows=NULL;
  EFI_STATUS rc=st->BootServices->LoadImage(FALSE,image,(EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)option+offset),NULL,0,&windows);
  status("windows_loadimage_status=",rc);
  if(rc==EFI_NOT_FOUND){
    out("windows_loadimage_fallback=matching_boot_partition\n");
    rc=load_windows_from_matching_esp(image,(const UINT8*)option,length,offset,&windows);
    status("windows_final_loadimage_status=",rc);
  }
  /* Windows can call ExitBootServices; close the USB file before StartImage. */
  if(LogFile){(void)LogFile->Flush(LogFile);(void)LogFile->Close(LogFile);LogFile=NULL;}
  if(!EFI_ERROR(rc)&&windows){
    rc=st->BootServices->StartImage(windows,NULL,NULL);
    status("windows_startimage_return_status=",rc);
    /* StartImage should not return when Windows launches successfully. */
    (void)st->BootServices->UnloadImage(windows);
  }
finish:
  if(option)st->BootServices->FreePool(option);
  if(LogFile){(void)LogFile->Flush(LogFile);(void)LogFile->Close(LogFile);LogFile=NULL;}
  return EFI_SUCCESS; /* Never ask firmware to reset, and never alter BootOrder. */
}
