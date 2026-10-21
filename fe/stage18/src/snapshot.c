/* RTX3080 FE Stage18 single-GPU released-state snapshot experiment for Stage15.
 * NO GPU PCI/MMIO writes, DMA, FLR, unlock or firmware updates.
 * DisconnectController may hang or disturb display: unsafe until proven on hardware.
 */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DriverBinding.h>
#define MAX_PCI 256U
#define MAX_GOP 64U
static EFI_GUID LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID FsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID DriverGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_SYSTEM_TABLE *ST;
static EFI_FILE_PROTOCOL *Log;
static UINTN DriverOwnerCount,ChildOwnerCount;
static BOOLEAN MissingDriverStop;
static UINTN slen(const CHAR8 *s){UINTN n=0;while(s[n]&&n<512)n++;return n;}
static void output(const CHAR8 *s){if(Log){UINTN n=slen(s);(void)Log->Write(Log,&n,(VOID*)s);}}
static void hx(UINT64 v, UINTN d){CHAR8 b[20];if(d>16)d=16;
  for(UINTN i=0;i<d;i++){UINTN x=(UINTN)((v>>((d-i-1)*4))&15);b[i]=(CHAR8)(x<10?'0'+x:'a'+x-10);}b[d]=0;output(b);}
static void ln(const CHAR8 *s,UINT64 v){output(s);output("0x");hx(v,16);output("\n");}
static void ls(const CHAR8 *s,EFI_STATUS v){ln(s,(UINT64)v);}
static void file_open(EFI_HANDLE image){
  static const CHAR16 name[]=L"\\RTX3080-STAGE18-SNAPSHOT.TXT";
  EFI_LOADED_IMAGE_PROTOCOL *li=NULL;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
  EFI_FILE_PROTOCOL *root=NULL;
  if(EFI_ERROR(ST->BootServices->HandleProtocol(image,&LoadedGuid,(VOID**)&li))||!li)return;
  if(EFI_ERROR(ST->BootServices->HandleProtocol(li->DeviceHandle,&FsGuid,(VOID**)&fs))||!fs)return;
  if(EFI_ERROR(fs->OpenVolume(fs,&root))||!root)return;
  EFI_STATUS r=root->Open(root,&Log,(CHAR16*)name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  (void)root->Close(root);if(EFI_ERROR(r))Log=NULL;
  if(Log)(void)Log->SetPosition(Log,0);
}
static BOOLEAN devpath_prefix(const EFI_DEVICE_PATH_PROTOCOL *prefix,const EFI_DEVICE_PATH_PROTOCOL *candidate){
  if(!prefix||!candidate)return FALSE;
  const UINT8 *a=(const UINT8*)prefix,*b=(const UINT8*)candidate;UINTN off=0;
  for(UINTN i=0;i<30;i++){
    if(off+4>1024)return FALSE;
    UINTN al=(UINTN)a[off+2]|((UINTN)a[off+3]<<8);
    UINTN bl=(UINTN)b[off+2]|((UINTN)b[off+3]<<8);
    if(al<4||bl<4||al>128||bl>128||off+al>1024||off+bl>1024)return FALSE;
    if((a[off]&0x7fU)==0x7fU)return a[off+1]==0xffU;
    if((b[off]&0x7fU)==0x7fU||al!=bl)return FALSE;
    for(UINTN j=0;j<al;j++)if(a[off+j]!=b[off+j])return FALSE;
    off+=al;
  }
  return FALSE;
}
static UINTN record_owners(EFI_HANDLE handle, EFI_GUID *guid,const CHAR8 *key, BOOLEAN *driver, BOOLEAN *child){
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *info=NULL;UINTN count=0;
  EFI_STATUS rc=ST->BootServices->OpenProtocolInformation(handle,guid,&info,&count);
  output(key);output("_query_status=0x");hx((UINT64)rc,16);output("\n");
  if(EFI_ERROR(rc)||count>128||(count && !info)){if(info)ST->BootServices->FreePool(info);return 0;}
  output(key);output("_count=0x");hx(count,4);output("\n");
  for(UINTN i=0;i<count;i++){
    output(key);output("_index=0x");hx(i,4);
    output(" attribute=0x");hx(info[i].Attributes,8);
    output(" open_count=0x");hx(info[i].OpenCount,8);
    output(" agent=0x");hx((UINTN)info[i].AgentHandle,16);
    output(" controller=0x");hx((UINTN)info[i].ControllerHandle,16);output("\n");
    if(info[i].Attributes&0x10U){
      *driver=TRUE;DriverOwnerCount++;
      EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;
      EFI_STATUS br=ST->BootServices->HandleProtocol(info[i].AgentHandle,&DriverGuid,(VOID**)&binding);
      output("stage18_driver_binding_status=0x");hx((UINT64)br,16);
      output(" has_stop=");hx(!EFI_ERROR(br)&&binding&&binding->Stop!=NULL,1);
      output(" image_handle=0x");hx((!EFI_ERROR(br)&&binding)?(UINTN)binding->ImageHandle:0,16);
      output("\n");
      if(EFI_ERROR(br)||!binding||!binding->Stop)MissingDriverStop=TRUE;
    }
    if(info[i].Attributes&0x08U){*child=TRUE;ChildOwnerCount++;}
  }
  if(info)ST->BootServices->FreePool(info);
  return count;
}
/* Capture ownership without opening a new BY_DRIVER relationship. */
static EFI_STATUS owner_state(EFI_HANDLE handle, EFI_GUID *guid, UINT32 *state){
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *info=NULL;UINTN n=0;
  EFI_STATUS rc=ST->BootServices->OpenProtocolInformation(handle,guid,&info,&n);
  if(EFI_ERROR(rc)||n>128){if(info)ST->BootServices->FreePool(info);return rc==EFI_SUCCESS?EFI_COMPROMISED_DATA:rc;}
  UINT32 bits=0;
  for(UINTN i=0;i<n;i++){
    if(info[i].Attributes&0x10U)bits|=1U;
    if(info[i].Attributes&0x08U)bits|=2U;
  }
  if(info)ST->BootServices->FreePool(info);*state=bits;return EFI_SUCCESS;
}
static EFI_STATUS gop_state(const EFI_DEVICE_PATH_PROTOCOL *gpu_path,UINTN *match,UINT32 *bits){
  EFI_HANDLE *handles=NULL;UINTN n=0;EFI_STATUS rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&n,&handles);
  if(EFI_ERROR(rc)||n>MAX_GOP||!handles){if(handles)ST->BootServices->FreePool(handles);return rc==EFI_SUCCESS?EFI_COMPROMISED_DATA:rc;}
  *match=0;*bits=0;
  for(UINTN i=0;i<n;i++){
    EFI_DEVICE_PATH_PROTOCOL *path=NULL;
    if(EFI_ERROR(ST->BootServices->HandleProtocol(handles[i],&PathGuid,(VOID**)&path))||!path)continue;
    if(devpath_prefix(gpu_path,path)){
      (*match)++;
      UINT32 b=0;if(EFI_ERROR(owner_state(handles[i],&GopGuid,&b))){ST->BootServices->FreePool(handles);return EFI_DEVICE_ERROR;}
      *bits|=b;
    }
  }
  ST->BootServices->FreePool(handles);
  return EFI_SUCCESS;
}

/* Diagnostic-only: does not enable memory decode or bus mastering. The BAR0
 * read operates through EfiPciIoProtocol and is skipped unless the expected
 * RTX 3080 FE MMIO aperture is already enabled. */
static void inspect_pci_state(EFI_HANDLE gpu,const CHAR8 *prefix){
  EFI_PCI_IO_PROTOCOL *p=NULL;
  EFI_STATUS s=ST->BootServices->HandleProtocol(gpu,&PciGuid,(VOID**)&p);
  output(prefix);output("pci_status=0x");hx((UINT64)s,16);output("\n");
  if(EFI_ERROR(s)||!p)return;
  UINT32 id=0xffffffffU,command=0xffffffffU,bar=0xffffffffU;
  s=p->Pci.Read(p,EfiPciIoWidthUint32,0,1,&id);
  output(prefix);output("pci_id_status=0x");hx((UINT64)s,16);output(" id=0x");hx(id,8);output("\n");
  if(EFI_ERROR(s)||id!=0x220610deU)return;
  s=p->Pci.Read(p,EfiPciIoWidthUint32,4,1,&command);
  output(prefix);output("command_status=0x");hx((UINT64)s,16);output(" command=0x");hx(command&0xffffU,4);output("\n");
  if(EFI_ERROR(s))return;
  s=p->Pci.Read(p,EfiPciIoWidthUint32,0x10,1,&bar);
  output(prefix);output("bar0_status=0x");hx((UINT64)s,16);output(" bar0=0x");hx(bar,8);output("\n");
  if(EFI_ERROR(s))return;
  UINT64 attrs=0;
  if(p->Attributes){
    s=p->Attributes(p,EfiPciIoAttributeOperationGet,0,&attrs);
    output(prefix);output("attributes_status=0x");hx((UINT64)s,16);output(" attributes=0x");hx(attrs,16);output("\n");
  }
  if((command&2U)==0 || (bar&0xfffffff0U)!=0xfb000000U){
    output(prefix);output("mmio_result=SKIP_DECODE_OR_BAR\n");return;
  }
  static const UINT32 reg[]={0U,0x00823804U,0x0082381cU,0x00823820U};
  static const CHAR8 *names[]={"chip","plm","ss0","ss1"};
  for(UINTN i=0;i<4;i++){
    UINT32 val=0xffffffffU;
    s=p->Mem.Read(p,EfiPciIoWidthUint32,0,reg[i],1,&val);
    output(prefix);output(names[i]);output("_read_status=0x");hx((UINT64)s,16);
    output(" value=0x");hx(val,8);output("\n");
    if(EFI_ERROR(s))break;
  }
}

static EFI_STATUS log_flush(void){return Log?Log->Flush(Log):EFI_DEVICE_ERROR;}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *system){
  if(!system||!system->BootServices)return EFI_INVALID_PARAMETER;
  ST=system;file_open(image);
  BOOLEAN reconnect_verified=FALSE;
  if(!Log)return EFI_DEVICE_ERROR;
  output("RTX3080 FE STAGE18 RELEASED-STATE PCI/MMIO SNAPSHOT / RECONNECT\n");
  output("NO PCI/MMIO WRITES; no DMA/FLR/unlock. UEFI driver disconnect/reconnect occurs.\n");
  EFI_HANDLE *handles=NULL;UINTN count=0,matched=0;EFI_HANDLE gpu=NULL;
  EFI_DEVICE_PATH_PROTOCOL *gpu_path=NULL;BOOLEAN pci_driver=FALSE,pci_child=FALSE;
  EFI_STATUS rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&count,&handles);
  ls("stage18_pci_enum_status=",rc);ln("stage18_pci_handles=",count);
  if(EFI_ERROR(rc)||!handles||count>MAX_PCI)goto finish;
  for(UINTN i=0;i<count;i++){
    EFI_PCI_IO_PROTOCOL *p=NULL;UINT32 id=0,cl=0,sub=0,cmd=0;
    if(EFI_ERROR(ST->BootServices->HandleProtocol(handles[i],&PciGuid,(VOID**)&p))||!p)continue;
    if(EFI_ERROR(p->Pci.Read(p,EfiPciIoWidthUint32,0,1,&id))||id!=0x220610deU)continue;
    if(EFI_ERROR(p->Pci.Read(p,EfiPciIoWidthUint32,8,1,&cl))||((cl>>24)&0xffU)!=3U)continue;
    if(EFI_ERROR(p->Pci.Read(p,EfiPciIoWidthUint32,0x2c,1,&sub))||sub!=0x146710deU)continue;
    if(EFI_ERROR(p->Pci.Read(p,EfiPciIoWidthUint32,4,1,&cmd)))continue;
    UINTN seg=0,bus=0,dev=0,fun=0;
    if(EFI_ERROR(p->GetLocation(p,&seg,&bus,&dev,&fun)))continue;
    if(seg!=0||bus!=0x26U||dev!=0||fun!=0|| (cmd&0xffffU)!=0x7U)continue;
    matched++;gpu=handles[i];ln("stage18_target_segment=",seg);ln("stage18_target_bus=",bus);
    ln("stage18_target_device=",dev);ln("stage18_target_function=",fun);
    ln("stage18_target_command=",cmd&0xffffU);ln("stage18_target_subsystem=",sub);
    ls("stage18_target_devicepath_status=",ST->BootServices->HandleProtocol(gpu,&PathGuid,(VOID**)&gpu_path));
    record_owners(gpu,&PciGuid,"stage18_pci_owner",&pci_driver,&pci_child);
  }
  ST->BootServices->FreePool(handles);handles=NULL;
  ln("stage18_target_matches=",matched);
  if(matched!=1||!gpu||!gpu_path){output("stage18_result=SKIP_GPU_IDENTITY\n");goto finish;}
  BOOLEAN gop_driver=FALSE,gop_child=FALSE;UINTN nGops=0,matched_gop=0;
  rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&nGops,&handles);
  ls("stage18_gop_enum_status=",rc);ln("stage18_gop_handles=",nGops);
  if(!EFI_ERROR(rc)&&handles&&nGops<=MAX_GOP){
    for(UINTN i=0;i<nGops;i++){
      EFI_DEVICE_PATH_PROTOCOL *path=NULL;
      if(EFI_ERROR(ST->BootServices->HandleProtocol(handles[i],&PathGuid,(VOID**)&path)))continue;
      if(devpath_prefix(gpu_path,path)){
        matched_gop++;ln("stage18_matching_gop_handle=",(UINTN)handles[i]);
        record_owners(handles[i],&GopGuid,"stage18_gop_owner",&gop_driver,&gop_child);
      }
    }
  }
  if(handles)ST->BootServices->FreePool(handles);
  ln("stage18_target_gop_routes=",matched_gop);
  ln("stage18_pci_driver_bound=",pci_driver);ln("stage18_pci_child_controller=",pci_child);
  ln("stage18_gop_driver_bound=",gop_driver);ln("stage18_gop_child_controller=",gop_child);
  ln("stage18_driver_owner_total=",DriverOwnerCount);
  ln("stage18_child_owner_total=",ChildOwnerCount);
  ln("stage18_missing_driver_stop=",MissingDriverStop);
  /* Fail closed unless the exact Stage16-observed sole GPU topology is present. */
  if(matched!=1||matched_gop!=1||nGops!=1||
     !pci_driver||!pci_child||!gop_driver||gop_child||
     MissingDriverStop||DriverOwnerCount!=2||ChildOwnerCount!=1||
     gpu_path==NULL) {
    output("stage18_result=SKIP_UNEXPECTED_DRIVER_TOPOLOGY\n");goto finish;
  }
  /* A log flush is mandatory BEFORE any controller ownership transition. */
  output("stage18_disconnection_attempt=1\n");
  EFI_STATUS preflush=log_flush();
  ls("stage18_pre_disconnect_flush_status=",preflush);
  if(EFI_ERROR(preflush)){output("stage18_result=SKIP_LOG_FLUSH_FAILED\n");goto finish;}
  EFI_STATUS disconnect_status=ST->BootServices->DisconnectController(gpu,NULL,NULL);
  ls("stage18_disconnect_status=",disconnect_status);
  /* The PCI device handle must survive; only its graphics driver should stop. */
  UINT32 released_bits=0;EFI_STATUS owner_rc=owner_state(gpu,&PciGuid,&released_bits);
  ls("stage18_released_owner_status=",owner_rc);
  ln("stage18_released_owner_bits=",released_bits);
  /* Read post-detach state while the GPU graphics driver is disconnected. */
  if(!EFI_ERROR(disconnect_status)&&!EFI_ERROR(owner_rc)&&released_bits==0U) {
    output("stage18_snapshot_phase=RELEASED\n");
    inspect_pci_state(gpu,"stage18_released_");
  } else {output("stage18_snapshot_phase=SKIPPED_RELEASE_FAILED\n");}
  (void)log_flush();
  /* Reconnect even if disconnect returned error: it might have partially stopped a driver. */
  output("stage18_reconnection_attempt=1\n");(void)log_flush();
  EFI_STATUS connect_status=ST->BootServices->ConnectController(gpu,NULL,NULL,TRUE);
  ls("stage18_connect_status=",connect_status);
  (void)log_flush();
  UINT32 restored_bits=0,gop_bits=0;UINTN restored_gops=0;
  EFI_STATUS restored_rc=owner_state(gpu,&PciGuid,&restored_bits);
  EFI_DEVICE_PATH_PROTOCOL *new_gpu_path=NULL;
  EFI_STATUS dp_rc=ST->BootServices->HandleProtocol(gpu,&PathGuid,(VOID**)&new_gpu_path);
  EFI_STATUS gop_rc=EFI_NOT_FOUND;
  if(!EFI_ERROR(dp_rc)&&new_gpu_path)gop_rc=gop_state(new_gpu_path,&restored_gops,&gop_bits);
  inspect_pci_state(gpu,"stage18_restored_");
  ls("stage18_restored_devicepath_status=",dp_rc);
  ls("stage18_restored_pci_status=",restored_rc);
  ln("stage18_restored_owner_bits=",restored_bits);
  ls("stage18_restored_gop_status=",gop_rc);
  ln("stage18_restored_gop_count=",restored_gops);
  ln("stage18_restored_gop_owner_bits=",gop_bits);
  if(!EFI_ERROR(disconnect_status)&&!EFI_ERROR(owner_rc)&&released_bits==0U&&
      !EFI_ERROR(connect_status)&&!EFI_ERROR(restored_rc)&&
      (restored_bits&3U)==3U&&!EFI_ERROR(gop_rc)&&restored_gops==1&&
      (gop_bits&1U)==1U){
    reconnect_verified=TRUE;
    output("stage18_result=PASS_RELEASE_RECONNECT_SNAPSHOT_NO_UNLOCK\n");
  } else {
    output("stage18_result=FAIL_OR_PARTIAL_RELEASE_RECONNECT\n");
  }
finish:
  output(reconnect_verified?"stage18_child_return=EFI_SUCCESS_NO_UNLOCK\n":"stage18_child_return=EFI_ACCESS_DENIED_NO_UNLOCK\n");
  (void)Log->Flush(Log);(void)Log->Close(Log);Log=NULL;
  /* A successful return only means graphics driver rebinding worked; NEVER implies unlock. */
  return reconnect_verified?EFI_SUCCESS:EFI_ACCESS_DENIED;
}
