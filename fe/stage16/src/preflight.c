/* RTX3080 FE Stage16, read-only child preflight for Stage15 bridge.
 * Fail intentionally with EFI_ACCESS_DENIED or EFI_UNSUPPORTED.
 * No PCI config writes/MMIO/DMA/FLR, DisconnectController or ConnectController.
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
static UINTN slen(const CHAR8 *s){UINTN n=0;while(s[n]&&n<512)n++;return n;}
static void output(const CHAR8 *s){if(Log){UINTN n=slen(s);(void)Log->Write(Log,&n,(VOID*)s);}}
static void hx(UINT64 v, UINTN d){CHAR8 b[20];if(d>16)d=16;
  for(UINTN i=0;i<d;i++){UINTN x=(UINTN)((v>>((d-i-1)*4))&15);b[i]=(CHAR8)(x<10?'0'+x:'a'+x-10);}b[d]=0;output(b);}
static void ln(const CHAR8 *s,UINT64 v){output(s);output("0x");hx(v,16);output("\n");}
static void ls(const CHAR8 *s,EFI_STATUS v){ln(s,(UINT64)v);}
static void file_open(EFI_HANDLE image){
  static const CHAR16 name[]=L"\\RTX3080-STAGE16-PREFLIGHT.TXT";
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
      *driver=TRUE;
      EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;
      EFI_STATUS br=ST->BootServices->HandleProtocol(info[i].AgentHandle,&DriverGuid,(VOID**)&binding);
      output("stage16_driver_binding_status=0x");hx((UINT64)br,16);
      output(" has_stop=");hx(!EFI_ERROR(br)&&binding&&binding->Stop!=NULL,1);
      output(" image_handle=0x");hx((!EFI_ERROR(br)&&binding)?(UINTN)binding->ImageHandle:0,16);
      output("\n");
    }
    if(info[i].Attributes&0x08U)*child=TRUE;
  }
  if(info)ST->BootServices->FreePool(info);
  return count;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *system){
  if(!system||!system->BootServices)return EFI_INVALID_PARAMETER;
  ST=system;file_open(image);
  if(!Log)return EFI_DEVICE_ERROR;
  output("RTX3080 FE STAGE16 READ-ONLY HEADLESS CHILD PREFLIGHT\n");
  output("No GPU MMIO/PCI writes, DMA, FLR, driver disconnect or unlock are performed.\n");
  EFI_HANDLE *handles=NULL;UINTN count=0,matched=0;EFI_HANDLE gpu=NULL;
  EFI_DEVICE_PATH_PROTOCOL *gpu_path=NULL;BOOLEAN pci_driver=FALSE,pci_child=FALSE;
  EFI_STATUS rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&count,&handles);
  ls("stage16_pci_enum_status=",rc);ln("stage16_pci_handles=",count);
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
    matched++;gpu=handles[i];ln("stage16_target_segment=",seg);ln("stage16_target_bus=",bus);
    ln("stage16_target_device=",dev);ln("stage16_target_function=",fun);
    ln("stage16_target_command=",cmd&0xffffU);ln("stage16_target_subsystem=",sub);
    ls("stage16_target_devicepath_status=",ST->BootServices->HandleProtocol(gpu,&PathGuid,(VOID**)&gpu_path));
    record_owners(gpu,&PciGuid,"stage16_pci_owner",&pci_driver,&pci_child);
  }
  ST->BootServices->FreePool(handles);handles=NULL;
  ln("stage16_target_matches=",matched);
  if(matched!=1||!gpu||!gpu_path){output("stage16_result=SKIP_GPU_IDENTITY\n");goto finish;}
  BOOLEAN gop_driver=FALSE,gop_child=FALSE;UINTN nGops=0,matched_gop=0;
  rc=ST->BootServices->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&nGops,&handles);
  ls("stage16_gop_enum_status=",rc);ln("stage16_gop_handles=",nGops);
  if(!EFI_ERROR(rc)&&handles&&nGops<=MAX_GOP){
    for(UINTN i=0;i<nGops;i++){
      EFI_DEVICE_PATH_PROTOCOL *path=NULL;
      if(EFI_ERROR(ST->BootServices->HandleProtocol(handles[i],&PathGuid,(VOID**)&path)))continue;
      if(devpath_prefix(gpu_path,path)){
        matched_gop++;ln("stage16_matching_gop_handle=",(UINTN)handles[i]);
        record_owners(handles[i],&GopGuid,"stage16_gop_owner",&gop_driver,&gop_child);
      }
    }
  }
  if(handles)ST->BootServices->FreePool(handles);
  ln("stage16_target_gop_routes=",matched_gop);
  ln("stage16_pci_driver_bound=",pci_driver);ln("stage16_pci_child_controller=",pci_child);
  ln("stage16_gop_driver_bound=",gop_driver);ln("stage16_gop_child_controller=",gop_child);
  if(pci_child||gop_child)output("stage16_result=SKIP_ACTIVE_CHILD_CONTROLLER\n");
  else if(pci_driver||gop_driver||matched_gop)output("stage16_result=SKIP_ACTIVE_DISPLAY_DRIVER\n");
  else output("stage16_result=SKIP_NO_VERIFIED_RECOVERY_PATH\n");
finish:
  output("stage16_child_return=EFI_ACCESS_DENIED\n");
  (void)Log->Flush(Log);(void)Log->Close(Log);Log=NULL;
  return EFI_ACCESS_DENIED; /* Stage15 must report failure and boot Windows. */
}
