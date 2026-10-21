/* RTX 3080 FE Stage15: headless boot bridge with explicit one-shot or persistent-auto opt-in.
 * AUTO mode places a durable attempt lock before starting the external child;
 * a hang leaves the lock and prevents repeat GPU writes on the next USB boot.
 * This bridge does no GPU writes; the child can modify the GPU.
 * Whether that module can operate safely is independent and NOT established here.
 * All UEFI calls are through the firmware's provided tables (no CRT imports).
 */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Guid/GlobalVariable.h>
#include "guard.h"

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
static S14Observation S14;

static UINTN textlen(const CHAR8 *s){ UINTN i=0;while(s[i]&&i<2048)i++;return i; }
static void out(const CHAR8 *s){
  /* Headless: log to USB, never require firmware console/GOP output. */
  if(LogFile){UINTN n=textlen(s);(void)LogFile->Write(LogFile,&n,(VOID*)s);}
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
/* Strict observational owner check; 0x10=BY_DRIVER, 0x08=BY_CHILD_CONTROLLER.
 * This function NEVER invokes OpenProtocol(EXCLUSIVE) or DisconnectController. */
static void stage14_owner_probe(EFI_HANDLE h,EFI_GUID *guid,BOOLEAN pci_owner){
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN count=0;
  EFI_STATUS rc=ST->BootServices->OpenProtocolInformation(h,guid,&entries,&count);
  if(!EFI_ERROR(rc)&&count<=128){
    if(pci_owner)S14.pci_owner_query_ok=1;
    else S14.gop_owner_query_ok=1;
    for(UINTN i=0;i<count;i++){
      UINT32 attr=entries[i].Attributes;
      if(attr&0x10U){if(pci_owner)S14.pci_bound_driver=1;else S14.gop_bound_driver=1;}
      if(attr&0x08U){if(pci_owner)S14.pci_child_controller=1;else S14.gop_child_controller=1;}
      if(attr&0x20U){if(pci_owner)S14.pci_bound_driver=1;else S14.gop_bound_driver=1;}
    }
  }
  if(entries)ST->BootServices->FreePool(entries);
}
static void stage15_log_observation(VOID){
  S14.recovery_proven=0;
  S14.stage12_integrated_and_verified=0;
  number("stage15_legacy_guard_mask=",s14_gate(&S14));
  number("stage15_gpu_identity_ok=",S14.pci_data_ok && S14.location_ok &&
    S14.device_id==0x220610deu && S14.subsystem_id==0x146710deu);
  number("stage15_gop_routes=",S14.gop_routes);
  number("stage15_driver_bound=",S14.pci_bound_driver||S14.gop_bound_driver);
  out("stage15_note=legacy_guard_is_diagnostic_external_image_not_proven_safe\n");
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
      EFI_STATUS locrc=pci->GetLocation(pci,&seg,&bus,&dev,&func);
      status("fe_location_status=",locrc);
      number("fe_segment=",seg);number("fe_bus=",bus);number("fe_device=",dev);number("fe_function=",func);
      EFI_STATUS cmdrc=pci->Pci.Read(pci,EfiPciIoWidthUint32,4,1,&cmd);
      status("fe_cmd_read_status=",cmdrc);
      number("fe_pci_command=",cmd&0xffffU);
      UINT32 subsys=0;
      EFI_STATUS subrc=pci->Pci.Read(pci,EfiPciIoWidthUint32,0x2c,1,&subsys);
      status("fe_subsystem_read_status=",subrc);number("fe_subsystem_id=",subsys);
      S14.device_id=id;S14.subsystem_id=subsys;
      S14.pci_command=(UINT16)(cmd&0xffffU);
      S14.location_ok=(!EFI_ERROR(locrc)&&
                        seg==0&&bus==0x26U&&dev==0&&func==0);
      S14.pci_data_ok=(!EFI_ERROR(cmdrc)&&!EFI_ERROR(subrc));
      stage14_owner_probe(pcis[i],&PciGuid,TRUE);
      UINT32 bar=0;status("fe_bar0_read_status=",pci->Pci.Read(pci,EfiPciIoWidthUint32,0x10,1,&bar));
      number("fe_bar0_raw=",bar);
      status("fe_device_path_status=",ST->BootServices->HandleProtocol(pcis[i],&PathGuid,(VOID**)&gpu_path));
      inspect_owners(pcis[i],&PciGuid,"fe_pci");
      /* Never disconnect or open this graphics driver EXCLUSIVE. */
    }
  }
  number("nvidia_fe_display_count=",matched);number("amd_display_count=",amd);
  S14.matched_count=(UINT8)(matched>255?255:matched);
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
    if(is_gpu){matched_gop++;inspect_owners(gops[i],&GopGuid,"fe_gop");
                stage14_owner_probe(gops[i],&GopGuid,FALSE);}
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
  S14.gop_routes=(UINT8)(matched_gop>255?255:matched_gop);
  S14.other_display_count=(UINT8)(amd>255?255:amd);
  if(matched==1&&amd==0&&matched_gop>=1)out("sole_gpu_topology=OBSERVED\n");
  else out("sole_gpu_topology=NOT_CONFIRMED\n");
}
static void make_log_name(CHAR16 *name){
  static const CHAR16 prefix[]=L"\\RTX3080-STAGE15-";
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

/* One-shot opt-in marker is consumed before launching any executable. */
static EFI_STATUS take_one_shot_marker(EFI_HANDLE image){
  static const CHAR16 name[]=L"\\RTX3080-STAGE15-ARM.ONCE";
  static const CHAR8 challenge[]="I_ACCEPT_GPU_HANG_RISK";
  EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
  EFI_FILE_PROTOCOL *root=NULL,*file=NULL;
  EFI_STATUS rc=ST->BootServices->HandleProtocol(image,&LoadedGuid,(VOID**)&loaded);
  if(EFI_ERROR(rc)||!loaded)return EFI_NOT_FOUND;
  rc=ST->BootServices->HandleProtocol(loaded->DeviceHandle,&FsGuid,(VOID**)&fs);
  if(EFI_ERROR(rc)||!fs)return EFI_NOT_FOUND;
  rc=fs->OpenVolume(fs,&root);
  if(EFI_ERROR(rc)||!root)return EFI_NOT_FOUND;
  rc=root->Open(root,&file,(CHAR16*)name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
  if(EFI_ERROR(rc)||!file){(void)root->Close(root);return EFI_NOT_FOUND;}
  CHAR8 content[sizeof(challenge)];for(UINTN i=0;i<sizeof(content);i++)content[i]=0;
  UINTN length=sizeof(content)-1;
  rc=file->Read(file,&length,content);
  BOOLEAN matching=!EFI_ERROR(rc) && length==sizeof(challenge)-1;
  for(UINTN i=0;matching&&i<sizeof(challenge)-1;i++)
    if(content[i]!=challenge[i])matching=FALSE;
  if(!matching){(void)file->Close(file);(void)root->Close(root);return EFI_SECURITY_VIOLATION;}
  /* If Delete fails, REFUSE the attempt: otherwise each failed boot could re-trigger it. */
  rc=file->Delete(file);
  (void)root->Close(root);
  return rc;
}
/* Optional persistent USB auto-arm. Fail closed when the enable marker is malformed,
 * a previous attempt lock remains, or file I/O cannot be durably flushed.
 * EFI file Delete closes the handle; only these two project-owned marker files
 * may be created/deleted by this code.
 */
static const CHAR16 AutoEnableName[]=L"\\RTX3080-STAGE15-AUTO.ON";
static const CHAR16 AutoPendingName[]=L"\\RTX3080-STAGE15-AUTO.PENDING";
static const CHAR8 AutoEnableToken[]="I_ACCEPT_REPEAT_GPU_HANG_RISK";
static const CHAR8 AutoPendingToken[]="STAGE15_AUTO_ATTEMPT_IN_PROGRESS";

static EFI_STATUS usb_root(EFI_HANDLE image,EFI_FILE_PROTOCOL **root){
  EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
  *root=NULL;
  EFI_STATUS rc=ST->BootServices->HandleProtocol(image,&LoadedGuid,(VOID**)&loaded);
  if(EFI_ERROR(rc)||!loaded)return EFI_UNSUPPORTED;
  rc=ST->BootServices->HandleProtocol(loaded->DeviceHandle,&FsGuid,(VOID**)&fs);
  if(EFI_ERROR(rc)||!fs)return EFI_UNSUPPORTED;
  return fs->OpenVolume(fs,root);
}
static EFI_STATUS marker_exact(EFI_FILE_PROTOCOL *root,const CHAR16 *name,
                               const CHAR8 *token,UINTN tokenlen){
  EFI_FILE_PROTOCOL *file=NULL;
  EFI_STATUS rc=root->Open(root,&file,(CHAR16*)name,EFI_FILE_MODE_READ,0);
  if(EFI_ERROR(rc)||!file)return rc;
  CHAR8 buf[64]={0};UINTN n=sizeof(buf);
  if(tokenlen>=sizeof(buf)){(void)file->Close(file);return EFI_INVALID_PARAMETER;}
  rc=file->Read(file,&n,buf);
  if(!EFI_ERROR(rc)){
    if(n!=tokenlen)rc=EFI_SECURITY_VIOLATION;
    else for(UINTN i=0;i<tokenlen;i++)if(buf[i]!=token[i]){rc=EFI_SECURITY_VIOLATION;break;}
  }
  (void)file->Close(file);
  return rc;
}
/* return EFI_NOT_FOUND if auto mode is not enabled; on EFI_SUCCESS the lock
 * has already been written and flushed, so we may try exactly one child.
 */
static EFI_STATUS auto_take_lock(EFI_HANDLE image){
  EFI_FILE_PROTOCOL *root=NULL;
  EFI_STATUS rc=usb_root(image,&root);
  if(EFI_ERROR(rc)||!root)return rc;
  rc=marker_exact(root,AutoEnableName,AutoEnableToken,sizeof(AutoEnableToken)-1);
  if(rc!=EFI_SUCCESS){(void)root->Close(root);return rc;}
  EFI_FILE_PROTOCOL *prior=NULL;
  rc=root->Open(root,&prior,(CHAR16*)AutoPendingName,EFI_FILE_MODE_READ,0);
  if(!EFI_ERROR(rc)){
    if(prior)(void)prior->Close(prior);
    (void)root->Close(root);
    return EFI_ALREADY_STARTED; /* Previous boot not proven to return. */
  }
  if(rc!=EFI_NOT_FOUND){(void)root->Close(root);return rc;}
  EFI_FILE_PROTOCOL *pending=NULL;
  rc=root->Open(root,&pending,(CHAR16*)AutoPendingName,
      EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,EFI_FILE_ARCHIVE);
  if(EFI_ERROR(rc)||!pending){(void)root->Close(root);return rc;}
  UINTN size=sizeof(AutoPendingToken)-1;
  rc=pending->Write(pending,&size,(VOID*)AutoPendingToken);
  if(!EFI_ERROR(rc)&&size!=sizeof(AutoPendingToken)-1)rc=EFI_DEVICE_ERROR;
  if(!EFI_ERROR(rc))rc=pending->Flush(pending);
  EFI_STATUS cr=pending->Close(pending);
  if(!EFI_ERROR(rc)&&EFI_ERROR(cr))rc=cr;
  (void)root->Close(root);
  return rc; /* On error any partial lock is intentionally retained. */
}
static EFI_STATUS auto_clear_lock(EFI_HANDLE image){
  EFI_FILE_PROTOCOL *root=NULL,*file=NULL;
  EFI_STATUS rc=usb_root(image,&root);
  if(EFI_ERROR(rc)||!root)return rc;
  rc=root->Open(root,&file,(CHAR16*)AutoPendingName,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
  if(!EFI_ERROR(rc)&&file)rc=file->Delete(file);
  (void)root->Close(root);
  return rc;
}
/* Build a complete USB-device/File path. Path object is freed after LoadImage. */
static EFI_STATUS load_usb_child(EFI_HANDLE image,EFI_HANDLE *child){
  static const CHAR16 child_path[]=L"\\EFI\\BOOT\\RTX3080-UNLOCK.EFI";
  EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
  EFI_DEVICE_PATH_PROTOCOL *path=NULL;
  EFI_STATUS rc=ST->BootServices->HandleProtocol(image,&LoadedGuid,(VOID**)&loaded);
  if(EFI_ERROR(rc)||!loaded)return EFI_UNSUPPORTED;
  rc=ST->BootServices->HandleProtocol(loaded->DeviceHandle,&PathGuid,(VOID**)&path);
  if(EFI_ERROR(rc)||!path)return EFI_UNSUPPORTED;
  UINTN prefix=0;
  for(UINTN node=0;node<64;node++){
    const UINT8 *p=(const UINT8*)path+prefix;
    UINTN n=(UINTN)p[2]|((UINTN)p[3]<<8);
    if(n<4||n>255||prefix+n>1024)return EFI_COMPROMISED_DATA;
    if((p[0]&0x7fU)==0x7fU){if(p[1]!=0xffU||n!=4)return EFI_COMPROMISED_DATA;break;}
    prefix+=n;
    if(node==63)return EFI_COMPROMISED_DATA;
  }
  UINTN filebytes=4+sizeof(child_path),total=prefix+filebytes+4;
  UINT8 *full=NULL;
  rc=ST->BootServices->AllocatePool(EfiLoaderData,total,(VOID**)&full);
  if(EFI_ERROR(rc))return rc;
  for(UINTN i=0;i<prefix;i++)full[i]=((UINT8*)path)[i];
  full[prefix]=0x04;full[prefix+1]=0x04;
  full[prefix+2]=(UINT8)(filebytes&0xff);full[prefix+3]=(UINT8)(filebytes>>8);
  for(UINTN i=0;i<sizeof(child_path);i++)full[prefix+4+i]=((UINT8*)child_path)[i];
  full[prefix+filebytes]=0x7f;full[prefix+filebytes+1]=0xff;
  full[prefix+filebytes+2]=4;full[prefix+filebytes+3]=0;
  rc=ST->BootServices->LoadImage(FALSE,image,(EFI_DEVICE_PATH_PROTOCOL*)full,NULL,0,child);
  (void)ST->BootServices->FreePool(full);
  return rc;
}
static void run_child_once(EFI_HANDLE image){
  if(!LogFile)return; /* Logging mandatory before any GPU-modifying child. */
  /* Explicit AUTO mode takes priority. It never consumes the one-shot marker. */
  EFI_STATUS a=auto_take_lock(image);
  status("stage15_auto_gate_status=",a);
  BOOLEAN auto_mode=FALSE;
  if(a==EFI_SUCCESS){
    auto_mode=TRUE;
    out("stage15_mode=AUTO_PERSISTENT_LOCKED\n");
  } else if(a==EFI_NOT_FOUND){
    EFI_STATUS m=take_one_shot_marker(image);
    status("stage15_marker_status=",m);
    if(m==EFI_NOT_FOUND){out("stage15_result=SKIPPED_NOT_ARMED\n");return;}
    if(m!=EFI_SUCCESS){out("stage15_result=FAILED_MARKER_NOT_CONSUMED\n");return;}
    out("stage15_mode=ONE_SHOT\n");
  } else if(a==EFI_ALREADY_STARTED){
    out("stage15_result=SKIPPED_AUTO_PENDING_PREVIOUS_ATTEMPT\n");return;
  } else {
    out("stage15_result=SKIPPED_AUTO_MARKER_OR_USB_ERROR\n");return;
  }
  out("stage15_attempt=1\n");
  out("stage15_result=ATTEMPT_STARTED_NOT_YET_RETURNED\n");
  EFI_STATUS flushed=LogFile->Flush(LogFile);
  if(EFI_ERROR(flushed)){
    out("stage15_result=FAILED_LOG_FLUSH\n");return; /* Keep auto lock. */
  }
  EFI_HANDLE child=NULL;
  EFI_STATUS rc=load_usb_child(image,&child);
  status("stage15_child_load_status=",rc);
  if(EFI_ERROR(rc)||!child){out("stage15_result=FAILED_CHILD_LOAD\n");return;}
  flushed=LogFile->Flush(LogFile);
  if(EFI_ERROR(flushed)){
    out("stage15_result=FAILED_LOG_FLUSH\n");
    (void)ST->BootServices->UnloadImage(child);return;
  }
  rc=ST->BootServices->StartImage(child,NULL,NULL);
  out("stage15_child_returned=1\n");
  status("stage15_child_startimage_return=",rc);
  (void)ST->BootServices->UnloadImage(child);
  if(EFI_ERROR(rc)){
    out("stage15_result=FAILED_CHILD_RETURNED\n");
    if(auto_mode)out("stage15_auto_lock=RETAINED_AFTER_CHILD_FAILURE\n");
  } else {
    out("stage15_result=CHILD_RETURNED_SUCCESS_UNLOCK_UNVERIFIED\n");
    if(auto_mode){
      /* Clear ONLY after child EFI_SUCCESS. This does not prove Windows
       * handoff or NVIDIA driver health. USB removal is recovery on failure.
       */
      flushed=LogFile->Flush(LogFile);
      if(EFI_ERROR(flushed)){
        out("stage15_auto_lock=RETAINED_LOG_ERROR\n");
      } else {
        EFI_STATUS cleared=auto_clear_lock(image);
        status("stage15_auto_lock_clear_status=",cleared);
        if(!EFI_ERROR(cleared))out("stage15_auto=REARMED_FOR_NEXT_USB_BOOT\n");
        else out("stage15_auto_lock=RETAINED_CLEAR_ERROR\n");
      }
    }
  }
  (void)LogFile->Flush(LogFile);
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *st){
  if(!st||!st->BootServices||!st->RuntimeServices)return EFI_INVALID_PARAMETER;
  ST=st;open_log(image);
  out("RTX3080 FE STAGE15 HEADLESS USB BRIDGE\n");
  out("BRIDGE DOES NO GPU WRITES; EXTERNAL UNLOCK IMAGE IS SEPARATE AND UNVERIFIED\n");
  inspect_gpu();
  stage15_log_observation();
  VOID *option=NULL;UINTN length=0,offset=0;UINT16 id=0;
  BOOLEAN found=find_bootmgr(&option,&length,&id,&offset);
  out("windows_bootmgr_discovered=");hex(found,1);out("\n");
  if(!found){out("stage15_result=FAILED_WINDOWS_BOOTMGR_NOT_FOUND\n");goto finish;}
  /* PRELOAD Windows while GOP and PCI devices are untouched. */
  EFI_HANDLE windows=NULL;
  EFI_STATUS rc=st->BootServices->LoadImage(FALSE,image,(EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)option+offset),NULL,0,&windows);
  status("windows_loadimage_status=",rc);
  if(rc==EFI_NOT_FOUND){
    out("windows_loadimage_fallback=matching_boot_partition\n");
    rc=load_windows_from_matching_esp(image,(const UINT8*)option,length,offset,&windows);
    status("windows_final_loadimage_status=",rc);
  }
  if(EFI_ERROR(rc)||!windows){out("stage15_result=FAILED_WINDOWS_PRELOAD\n");goto finish;}
  out("stage15_windows_preloaded=1\n");
  if(LogFile){
    EFI_STATUS logrc=LogFile->Flush(LogFile);
    if(EFI_ERROR(logrc)){out("stage15_result=FAILED_PRELOAD_LOG_FLUSH\n");goto handoff;}
  }
  run_child_once(image);
handoff:
  out("stage15_windows_handoff=1\n");
  if(LogFile){(void)LogFile->Flush(LogFile);(void)LogFile->Close(LogFile);LogFile=NULL;}
  rc=st->BootServices->StartImage(windows,NULL,NULL);
  status("windows_startimage_return_status=",rc);
  (void)st->BootServices->UnloadImage(windows);
finish:
  if(option)st->BootServices->FreePool(option);
  if(LogFile){(void)LogFile->Flush(LogFile);(void)LogFile->Close(LogFile);LogFile=NULL;}
  return EFI_SUCCESS;
}
