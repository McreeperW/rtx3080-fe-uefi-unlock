#include "app.h"
#include "app_io.h"
#include "compat/compat.h"
#include "checks.h"
#include "profiles.h"
#include "fe_bar.h"
#include "fe_gop.h"
#include "fe_pcie.h"
#include "fe_cold.h"
#include "vendor/vbios_610.h"
#include "sha256.h"
#include "ownership.h"
#include "vendor/unlock_610.h"
#include "vendor/gfw_eval.h"
#include <Protocol/GraphicsOutput.h>
#include <IndustryStandard/Acpi.h>
#include <Guid/GlobalVariable.h>
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
  PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,GlobalGuid=EFI_GLOBAL_VARIABLE;
static EFI_SYSTEM_TABLE *Table;
static UINT32 InitialConfig[64];
static const char *Stage;
static GU_RELEASE_OWNER PendingOwner;
#ifdef GU_FE_EXPERIMENTAL
/* Special opt-in build: runtime identity still must be exact FE. */
static BOOLEAN FeMode=FALSE,FeDecodeChanged=FALSE;
static BOOLEAN FeDisplayAuditReadOnly(VOID);
#ifdef GU_FE_HEADLESS
static BOOLEAN FeHeadlessDisplayCheck(VOID);
#endif
#endif
static EFI_STATUS Step(EFI_STATUS Status) {
  GuLog("CHECK stage=%s status=%#llx\n",Stage,(unsigned long long)Status);
  return !GuLogReady() && !EFI_ERROR(Status)?EFI_DEVICE_ERROR:Status;
}
static VOID ConfigLog(const UINT32 *Config) {
  for(UINTN i=0;i<64;i+=4)GuLog("config[%02x]=%08x %08x %08x %08x\n",
    (unsigned)(i*4),Config[i],Config[i+1],Config[i+2],Config[i+3]);
}
static VOID GopInfoLog(UINTN Index,const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info) {
  GuLog("GOP[%u] format=%u size=%ux%u stride=%u\n",(unsigned)Index,
    (unsigned)Info->PixelFormat,Info->HorizontalResolution,Info->VerticalResolution,Info->PixelsPerScanLine);
}
static BOOLEAN ConsoleGopCheck(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop,UINTN Index,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Output) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode=Gop->Mode;
  if(mode->Info && mode->SizeOfInfo>=sizeof(*mode->Info)) {
    *Output=*mode->Info;GopInfoLog(Index,Output);return Output->PixelFormat==PixelBltOnly;
  }
  /* RUN02's console aggregator leaves SizeOfInfo zero. Never read an unsized
     Info pointer: QueryMode supplies its own sized, callee-allocated buffer.
     The caller restricts this fallback to a pathless, zero-framebuffer GOP. */
  if(mode->SizeOfInfo || !Gop->QueryMode)return FALSE;
  UINT32 number=mode->Mode;UINTN bytes=0;EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info=NULL;
  EFI_STATUS status=Gop->QueryMode(Gop,number,&bytes,&info);
  GuLog("GOP[%u] console QueryMode: status=%#llx mode=%u info_bytes=%zu\n",
    (unsigned)Index,(unsigned long long)status,number,(size_t)bytes);
  BOOLEAN valid=!EFI_ERROR(status) && info && bytes>=sizeof(*info);
  if(valid) {
    *Output=*info;GopInfoLog(Index,Output);
    valid=(info->PixelFormat==PixelBltOnly || info->PixelFormat==PixelRedGreenBlueReserved8BitPerColor ||
      info->PixelFormat==PixelBlueGreenRedReserved8BitPerColor) && Gop->Mode==mode && mode->Mode==number &&
      !mode->FrameBufferBase && !mode->FrameBufferSize;
  }
  if(info) {
    status=Gu.bs->FreePool(info);
    if(EFI_ERROR(status)){GuLog("GOP[%u] query FreePool failed: %#llx\n",(unsigned)Index,(unsigned long long)status);valid=FALSE;}
  }
  return valid;
}
static BOOLEAN DisplayCheck(VOID) {
#ifdef GU_FE_EXPERIMENTAL
#ifdef GU_FE_HEADLESS
  if(FeMode)return FeHeadlessDisplayCheck();
#else
  if(FeMode)return FeDisplayAuditReadOnly();
#endif
#endif
  EFI_HANDLE *handles=NULL;UINTN count=0,intel=0;BOOLEAN valid=TRUE;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION physical={0},forwarded={0};BOOLEAN forwarding=FALSE;
  EFI_STATUS status=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&handles);
  GuLog("GOP enumeration: status=%#llx handles=%zu\n",(unsigned long long)status,(size_t)count);
  if(EFI_ERROR(status) || !handles || !count || count>16){if(handles)GuFree(handles);return FALSE;}
  for(UINTN i=0;i<count;i++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;
    status=Gu.bs->HandleProtocol(handles[i],&GopGuid,(VOID **)&gop);
    if(EFI_ERROR(status) || !gop || !gop->Mode){
      GuLog("GOP[%u] invalid protocol/mode: status=%#llx\n",(unsigned)i,
        (unsigned long long)status);valid=FALSE;break;
    }
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode=gop->Mode;
    status=Gu.bs->HandleProtocol(handles[i],&PathGuid,(VOID **)&path);
    int route=!EFI_ERROR(status) && path?GuDisplayPath(path,256):0;
    GuLog("GOP[%u] path_status=%#llx route=%d framebuffer=%#llx bytes=%zu mode=%u max_mode=%u info_bytes=%zu console=%u\n",
      (unsigned)i,(unsigned long long)status,route,(unsigned long long)mode->FrameBufferBase,
      (size_t)mode->FrameBufferSize,mode->Mode,mode->MaxMode,(size_t)mode->SizeOfInfo,(unsigned)(handles[i]==Table->ConsoleOutHandle));
    if((status==EFI_UNSUPPORTED || status==EFI_NOT_FOUND) && !path && !mode->FrameBufferBase &&
       !mode->FrameBufferSize) {
      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION info;
      if(ConsoleGopCheck(gop,i,&info)) {
        if(info.PixelFormat==PixelBltOnly)continue;
        /* EDK2 ConSplitter forwards QueryMode to its sole physical GOP. Accept
           that RGB/BGR result only on the system console, then match it against
           the one validated Intel GOP after enumeration (handle order varies). */
        if(handles[i]==Table->ConsoleOutHandle && !forwarding) {
          forwarded=info;forwarding=TRUE;continue;
        }
      }
      valid=FALSE;break;
    }
    if(!mode->Info || mode->SizeOfInfo<sizeof(*mode->Info)){valid=FALSE;break;}
    GopInfoLog(i,mode->Info);
    if(EFI_ERROR(status) || !path || route!=1 || !mode->FrameBufferBase ||
       !mode->FrameBufferSize || mode->Info->PixelFormat>PixelBlueGreenRedReserved8BitPerColor ||
       !mode->Info->HorizontalResolution || !mode->Info->VerticalResolution ||
       mode->Info->PixelsPerScanLine<mode->Info->HorizontalResolution ||
       (UINT64)mode->Info->PixelsPerScanLine*mode->Info->VerticalResolution>mode->FrameBufferSize/4) {
      valid=FALSE;break;
    }
    physical=*mode->Info;intel++;
  }
  if(forwarding) {
    BOOLEAN match=intel==1 && forwarded.Version==physical.Version && forwarded.PixelFormat==physical.PixelFormat &&
      forwarded.HorizontalResolution==physical.HorizontalResolution && forwarded.VerticalResolution==physical.VerticalResolution &&
      forwarded.PixelsPerScanLine==physical.PixelsPerScanLine;
    GuLog("GOP console forwarded Intel match=%u\n",match);valid=valid && match;
  }
  GuFree(handles);GuLog("GOP result: valid=%u intel_physical=%zu\n",valid,(size_t)intel);
  return valid && intel==1 && GuLogReady();
}
#ifdef GU_FE_HEADLESS
/* A prior Stage19 shim MUST have disconnected the sole-GPU NVIDIA GOP.
   Fail closed if GPU graphics ownership reappears. No arbitrary GPU handoff. */
static BOOLEAN FeHeadlessDisplayCheck(VOID) {
  EFI_HANDLE *gops=NULL;
  UINTN n=0;
  EFI_STATUS s=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&n,&gops);
  if(gops)Gu.bs->FreePool(gops);
  if(!EFI_ERROR(s) && n!=0)return FALSE;
  if(EFI_ERROR(s) && s!=EFI_NOT_FOUND)return FALSE;
  if(!Gu.pci || !Gu.handle)return FALSE;
  UINT32 id=0,sub=0,cmd=0;
  UINTN seg=0,bus=0,dev=0,fun=0;
  if(EFI_ERROR(Gu.pci->GetLocation(Gu.pci,&seg,&bus,&dev,&fun)) ||
    seg!=0||bus!=0x26||dev||fun)return FALSE;
  if(EFI_ERROR(Gu.pci->Pci.Read(Gu.pci,EfiPciIoWidthUint32,0,1,&id)) || id!=0x220610deU ||
     EFI_ERROR(Gu.pci->Pci.Read(Gu.pci,EfiPciIoWidthUint32,0x2c,1,&sub)) || sub!=0x146710deU ||
     EFI_ERROR(Gu.pci->Pci.Read(Gu.pci,EfiPciIoWidthUint32,4,1,&cmd)) ||
     (cmd&1U) || (cmd&0xfff8U))return FALSE;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *owners=NULL;
  UINTN count=0;
  s=Gu.bs->OpenProtocolInformation(Gu.handle,&PciGuid,&owners,&count);
  if(EFI_ERROR(s)||count>64){if(owners)Gu.bs->FreePool(owners);return FALSE;}
  BOOLEAN clean=TRUE;
  for(UINTN i=0;i<count;i++)
    if(owners[i].Attributes & (EFI_OPEN_PROTOCOL_BY_DRIVER|EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER))clean=FALSE;
  if(owners)Gu.bs->FreePool(owners);
  return clean;
}
#endif
static EFI_STATUS ReadConfig(UINT32 *Config) {
  for(UINTN i=0;i<64;i++)if(EFI_ERROR(GuConfigRead(EfiPciIoWidthUint32,(UINT32)i*4,&Config[i]))) {
    GuLog("PCI config read failed: offset=%#x valid_bytes=%u; snapshot incomplete\n",(unsigned)(i*4),(unsigned)(i*4));
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
/*
 * FE-only AMD GOP provenance audit. Handles are only queried, not opened
 * exclusively; no DisconnectController, framebuffer access or PCI writes.
 * A successful AMD route is a diagnostic result, NOT permission to unlock.
 */
static BOOLEAN FeDisplayAuditReadOnly(VOID) {
  EFI_HANDLE *pci_handles=NULL,*gop_handles=NULL;
  UINTN pcicount=0,gopcount=0,amd_count=0,physical_count=0;
  const EFI_DEVICE_PATH_PROTOCOL *amd_path=NULL;
  BOOLEAN valid=TRUE;
  EFI_STATUS status=Gu.bs->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&pcicount,&pci_handles);
  GuLog("FE AMD PCI enumeration status=%#llx handles=%zu\n",
    (unsigned long long)status,(size_t)pcicount);
  if(EFI_ERROR(status) || !pci_handles || !pcicount || pcicount>256) {
    if(pci_handles)GuFree(pci_handles);
    return FALSE;
  }
  for(UINTN i=0;i<pcicount;i++) {
    EFI_PCI_IO_PROTOCOL *pci=NULL;
    UINT32 id=0,cls=0;
    EFI_STATUS read=Gu.bs->HandleProtocol(pci_handles[i],&PciGuid,(VOID **)&pci);
    if(!EFI_ERROR(read) && pci)
      read=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,1,&id);
    if(!EFI_ERROR(read) && pci && id==0x15dd1002U) {
      read=pci->Pci.Read(pci,EfiPciIoWidthUint32,8,1,&cls);
      if(!EFI_ERROR(read) && ((cls>>24)&0xffU)==0x03U) {
        EFI_DEVICE_PATH_PROTOCOL *path=NULL;
        UINTN segment=0,bus=0,device=0,function=0;
        EFI_STATUS loc=pci->GetLocation(pci,&segment,&bus,&device,&function);
        EFI_STATUS dp=Gu.bs->HandleProtocol(pci_handles[i],&PathGuid,(VOID **)&path);
        GuLog("FE AMD display: location=%#llx %zu:%zu:%zu.%zu path_status=%#llx\n",
          (unsigned long long)loc,(size_t)segment,(size_t)bus,(size_t)device,
          (size_t)function,(unsigned long long)dp);
        if(EFI_ERROR(loc) || EFI_ERROR(dp) || !path)valid=FALSE;
        else {amd_count++;amd_path=path;}
      }
    }
  }
  GuFree(pci_handles);
  if(!valid || amd_count!=1) {
    GuLog("FE AMD GOP audit refused: AMD display controllers=%zu valid=%u\n",
      (size_t)amd_count,(unsigned)valid);
    return FALSE;
  }
  status=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&gopcount,&gop_handles);
  GuLog("FE GOP enumeration status=%#llx handles=%zu\n",
    (unsigned long long)status,(size_t)gopcount);
  if(EFI_ERROR(status) || !gop_handles || !gopcount || gopcount>16) {
    if(gop_handles)GuFree(gop_handles);
    return FALSE;
  }
  for(UINTN i=0;i<gopcount;i++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;
    EFI_DEVICE_PATH_PROTOCOL *path=NULL;
    EFI_STATUS gp=Gu.bs->HandleProtocol(gop_handles[i],&GopGuid,(VOID **)&gop);
    EFI_STATUS dp=Gu.bs->HandleProtocol(gop_handles[i],&PathGuid,(VOID **)&path);
    BOOLEAN route=!EFI_ERROR(dp) && path && GuFeGopMatchesController(amd_path,path,256);
    BOOLEAN mode=!EFI_ERROR(gp) && gop && gop->Mode && gop->Mode->Info &&
      gop->Mode->SizeOfInfo>=sizeof(*gop->Mode->Info) &&
      gop->Mode->Info->HorizontalResolution && gop->Mode->Info->VerticalResolution &&
      gop->Mode->Info->PixelsPerScanLine>=gop->Mode->Info->HorizontalResolution &&
      gop->Mode->FrameBufferBase && gop->Mode->FrameBufferSize &&
      gop->Mode->Info->PixelFormat<=PixelBlueGreenRedReserved8BitPerColor &&
      (UINT64)gop->Mode->Info->PixelsPerScanLine*gop->Mode->Info->VerticalResolution<=
        gop->Mode->FrameBufferSize/4U;
    GuLog("FE GOP[%u]: route_matches_AMD=%u framebuffer_mode_valid=%u path_status=%#llx\n",
      (unsigned)i,(unsigned)route,(unsigned)mode,(unsigned long long)dp);
    if(route && mode)physical_count++;
    else valid=FALSE; /* refuse ambiguous or additional GOP handles */
  }
  GuFree(gop_handles);
  valid=valid && physical_count==1;
  GuLog("FE GOP AMD provenance: controllers=%zu physical_matches=%zu valid=%u\n",
    (size_t)amd_count,(size_t)physical_count,(unsigned)valid);
  return valid;
}

/* FE-only inventory: no PCI/MMIO writes, no driver disconnect, no DMA/FLR.
 * This runs before the legacy MSI-only transaction eligibility checks.
 * An observed FE is unconditionally rejected after logging.
 */
/*
 * FE live cold-state / VBIOS inspection. This deliberately does NOT enable
 * PCI memory decoding or claim ownership. It can run only when UEFI has
 * already enabled Memory Space and no GPU bus-mastering is active.
 * Success in this diagnostic is never permission to run the MSI transaction.
 */
static VOID FeColdPromReadOnly(const UINT32 Config[64], BOOLEAN BarMatches,
    BOOLEAN GopMatches, BOOLEAN PcieValid) {
  if (!BarMatches || !GopMatches || !PcieValid ||
      (Config[1] & 0x4U) || (Config[0x30U/4U] & 1U)) {
    GuLogText("FE cold/PROM: skipped, prerequisites fail; authorization=DENIED\n");
    return;
  }
  if ((Config[1] & 0x2U)==0) {
    GuLogText("FE cold/PROM: skipped, PCI memory decode disabled; left unchanged. Previous REPORT(9) is offline evidence only.\n");
    return;
  }
  UINT32 now[64];
  if (EFI_ERROR(ReadConfig(now)) || memcmp(now,Config,sizeof(now))) {
    GuLogText("FE cold/PROM: config changed; skip all MMIO\n");return;
  }
  const UINT32 offs[]={0,0x118128U,0x118234U,0x110100U,
    0x1fa828U,0x823804U,0x82381cU,0x823820U,0x1183a4U,
    0x824148U,0x8241e0U};
  UINT32 values[sizeof(offs)/sizeof(offs[0])]={0};
  for(UINTN i=0;i<sizeof(offs)/sizeof(offs[0]);++i) {
    EFI_STATUS st=Gu.pci->Mem.Read(Gu.pci,EfiPciIoWidthUint32,0,offs[i],1,&values[i]);
    if(EFI_ERROR(st)) {
      GuLog("FE cold/PROM: MMIO read failure offset=%#x status=%#llx; abort inspection\n",
        offs[i],(unsigned long long)st);
      return;
    }
  }
  GU_FE_COLD_SAMPLE sample={values[0],values[1],values[2],values[3],values[4],
    values[5],values[6],values[7],values[8],values[9],values[10]};
  GU_FE_COLD_ASSESSMENT a=GuFeAssessCold(&sample);
  GuLog("FE cold observed: chip=%#x GFW=%#x/%#x/%#x WPR2_HI=%#x PLM=%#x SS0=%#x SS1=%#x mmu=%#x fuse=%#x/%#x\n",
    sample.chip,sample.gfw_plm,sample.gfw_boot,sample.gfw_cpuctl,
    sample.wpr2_hi,sample.feature_plm,sample.ss0,sample.ss1,
    sample.plm_1183a4,sample.booter_fuse,sample.fwsec_fuse);
  GuLog("FE cold assessment: known_observation=%u gfw_idle=%u signature_slots=%u differs_from_MSI=%u authorization=DENIED\n",
    (unsigned)a.known_observation,(unsigned)a.gfw_idle,
    (unsigned)a.signature_slots_supported,(unsigned)a.differs_from_msi);
  if(!a.known_observation || !GuLogReady())return;
  UINT8 *prom=GuAlloc(1048576U);
  if(!prom){GuLogText("FE PROM: allocation failed\n");return;}
  EFI_STATUS read=EFI_SUCCESS;
  /* EfiPciIoWidthUint32; 4KiB chunks; PCI BAR0 read-only. */
  for(UINTN i=0;i<1048576U/4096U;i++) {
    read=Gu.pci->Mem.Read(Gu.pci,EfiPciIoWidthUint32,0,
      0x300000U+i*4096U,1024U,prom+i*4096U);
    if(EFI_ERROR(read))break;
  }
  if(EFI_ERROR(read)) {
    GuLog("FE PROM: read failed status=%#llx; no firmware action\n",
      (unsigned long long)read);
  } else {
    BOOLEAN fingerprint=GuHashMatches(prom,1048576U,
      "d2a902d96f78783c42e39c94bb241e3644f386196b35b9e0c7d0820945e5735d");
    struct gpu_unlock_fwsec fw={0};
    int parsed=fingerprint?gpu_unlock_vbios_fwsec(prom,1048576U,&fw):-1;
    BOOLEAN shape=parsed==0 && fw.image_off==0x4239cU && fw.image_size==0xde00U &&
      fw.dmem_size==0x800U && fw.ucode_id==9U && fw.engine_mask==0x400U &&
      fw.sig_versions==7U && fw.sig_count==3U && fw.sig_bytes==1152U;
    GuLog("FE PROM: pinned_FE_SHA256=%u parsed=%u fwsec_offset=%#x fwsec_image=%#x shape=%u authorization=DENIED\n",
      (unsigned)fingerprint,(unsigned)(parsed==0),fw.image_off,fw.image_size,(unsigned)shape);
  }
  GuFree(prom);
  GuLogText("FE cold/PROM read-only inspection complete. No GPU writes, DMA, FLR or firmware execution.\n");
}

static EFI_STATUS FeInventoryReadOnly(const UINT32 Config[64]) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *bars[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
  GU_FE_BAR_OBSERVATION observed = GuFeObserveBar0(Config, NULL);
  GuLog("FE inventory: PCI command=%#x BAR0 raw=%#x base=%#x decode=%u\n",
    observed.command, observed.bar0_raw, observed.bar0_base,
    (unsigned)observed.memory_decode);
  for (UINTN i=0; i<6; ++i) {
    EFI_STATUS status = Gu.pci->GetBarAttributes(
      Gu.pci, (UINT8)i, NULL, (VOID **)&bars[i]);
    GuLog("FE BAR%u query status=%#llx\n", (unsigned)i,
      (unsigned long long)status);
    if (!EFI_ERROR(status) && bars[i]) {
      EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r = bars[i];
      /* Only use fields after confirming the descriptor's fixed-size prefix. */
      if (r->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR &&
          r->Len == sizeof(*r)-3U) {
        GuLog("FE BAR%u resource: type=%u bits=%llu min=%#llx max=%#llx length=%#llx translation=%#llx\n",
          (unsigned)i, (unsigned)r->ResType,
          (unsigned long long)r->AddrSpaceGranularity,
          (unsigned long long)r->AddrRangeMin,
          (unsigned long long)r->AddrRangeMax,
          (unsigned long long)r->AddrLen,
          (unsigned long long)r->AddrTranslationOffset);
      } else GuLog("FE BAR%u resource: descriptor rejected\n", (unsigned)i);
      if(i==0) observed=GuFeObserveBar0(Config,r);
    }
    if(bars[i]) GuFree(bars[i]);
  }
  GuLog("FE BAR0 descriptor agrees with PCI config=%u; memory decode=%u\n",
    (unsigned)observed.descriptor_matches,
    (unsigned)observed.memory_decode);
  GU_FE_PCIE_OBSERVATION pcie=GuFeObservePcie(Config);
  GuLog("FE PCIe capability: chain_valid=%u offset=%#x FLR_supported=%u FLR_pending=%u\n",
    (unsigned)pcie.valid_chain,(unsigned)pcie.pcie_offset,
    (unsigned)pcie.flr_supported,(unsigned)pcie.flr_pending);
  BOOLEAN amd_gop=FeDisplayAuditReadOnly();
  GuLog("FE read-only preflight: BAR_match=%u AMD_GOP_match=%u FLR_cap=%u decode=%u; authorization=DENIED\n",
    (unsigned)observed.descriptor_matches,(unsigned)amd_gop,
    (unsigned)(pcie.valid_chain && pcie.flr_supported && !pcie.flr_pending),
    (unsigned)observed.memory_decode);
  FeColdPromReadOnly(Config,observed.descriptor_matches,amd_gop,
    pcie.valid_chain && pcie.flr_supported && !pcie.flr_pending);
  GuLogText("FE READ-ONLY INVENTORY COMPLETE. TRANSACTION BLOCKED; no MMIO writes performed.\n");
  return EFI_UNSUPPORTED;
}

static EFI_STATUS ConfigCheck(BOOLEAN Save) {
  UINT32 config[64];GU_CAPS caps;UINTN segment,bus,device,function;
  EFI_STATUS status=Gu.pci->GetLocation(Gu.pci,&segment,&bus,&device,&function);
  GuLog("target location: status=%#llx\n",(unsigned long long)status);
  if(EFI_ERROR(status))return status;
  status=ReadConfig(config);if(EFI_ERROR(status))return status;
  GU_PROFILE_KIND profile=GuIdentifyProfile(config,segment,bus,device,function);
  GuLog("PCI observed profile=%s BDF=%zu:%zu:%zu.%zu BAR0=%#x decode=%u\n",
    GuProfileName(profile),(size_t)segment,(size_t)bus,(size_t)device,
    (size_t)function,GuProfileBar0Base(config),
    (unsigned)GuProfileMemoryDecodeEnabled(config));
  if(profile==GU_PROFILE_FE_OBSERVED) {
#ifdef GU_FE_EXPERIMENTAL
    if(!FeMode)return FeInventoryReadOnly(config);
    GuLogText("EXPERIMENTAL RTX 3080 FE profile: transaction path enabled; may hang the GPU.\n");
#else
    ConfigLog(config);
    return FeInventoryReadOnly(config); /* never reach MSI unlock checks */
#endif
  }
  BOOLEAN checked=GuCheckConfig(config,segment,bus,device,function,&caps);
#ifdef GU_FE_EXPERIMENTAL
  BOOLEAN bar=FeMode ? (GuProfileBar0Base(config)!=0) : config[0x10/4]==0x84000000;
#else
  BOOLEAN bar=config[0x10/4]==0x84000000;
#endif
  BOOLEAN valid=checked && config[2]==0x030000a1 && config[0xc8/4]==0x00050011 &&
    bar && !(config[(caps.pcie+8)/4]&0x00200000U);
#ifdef GU_FE_EXPERIMENTAL
  if(FeMode && !Save && !GuProfileMemoryDecodeEnabled(config))valid=FALSE;
#endif
  BOOLEAN same=Save || !memcmp(config,InitialConfig,sizeof(config));
  if(Save || !valid || !same)ConfigLog(config);
  GuLog("target BDF=%zu:%zu:%zu.%zu checks=%u pm=%#x pcie=%#x msi=%#x msix=%#x\n",
    (size_t)segment,(size_t)bus,(size_t)device,(size_t)function,checked,
    (unsigned)caps.pm,(unsigned)caps.pcie,(unsigned)caps.msi,(unsigned)caps.msix);
  if(!valid)return EFI_UNSUPPORTED;
  if(!same)return EFI_NOT_READY;
  if(Save)memcpy(InitialConfig,config,sizeof(config));
  Gu.pm_cap=caps.pm;Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;
  return EFI_SUCCESS;
}
static EFI_STATUS RestoreDecode(VOID) {
  UINT32 current[64],normalized[64],after[64];UINTN segment,bus,device,function;
  if(!Gu.pci || !Gu.owned || !Gu.armed || !Gu.display_ok || Gu.started || Gu.allocations ||
    Gu.dma_count || Gu.fault || Gu.retained)return EFI_ACCESS_DENIED;
  EFI_STATUS status=Gu.pci->GetLocation(Gu.pci,&segment,&bus,&device,&function);
  if(EFI_ERROR(status))return status;
  status=ReadConfig(current);if(EFI_ERROR(status))return status;
  GuLogText("config before decode restore (256 valid bytes):\n");ConfigLog(current);
  if(!GuTargetConfig(current,segment,bus,device,function))return EFI_UNSUPPORTED;
  UINT32 missing=InitialConfig[1]&~current[1]&3U;
  memcpy(normalized,current,sizeof(normalized));normalized[1]|=missing;
  /* Restore only cleared, previously enabled IO/MEM bits. Every other bit,
     including BME, status, BAR, ROM and PM/capability state must be unchanged. */
  if(memcmp(normalized,InitialConfig,sizeof(normalized))) {
    GuLogText("decode restore refused: config differs beyond cleared original IO/MEM bits\n");
    return EFI_UNSUPPORTED;
  }
  if(!missing){GuLogText("decode restore not needed\n");return EFI_SUCCESS;}
  if(!Gu.pci->Attributes)return EFI_UNSUPPORTED;
  UINT64 needed=(missing&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(missing&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);
  UINT64 supported=0,attributes=0;
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&supported);
  GuLog("decode supported attributes: status=%#llx mask=%#llx\n",(unsigned long long)status,(unsigned long long)supported);
  if(EFI_ERROR(status))return status;
  if((supported&needed)!=needed)return EFI_UNSUPPORTED;
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attributes);
  GuLog("decode current attributes: status=%#llx mask=%#llx\n",(unsigned long long)status,(unsigned long long)attributes);
  if(EFI_ERROR(status))return status;
  GuLog("decode restore intent: saved_command=%#x current_command=%#x missing=%#x attributes=%#llx\n",
    InitialConfig[1]&0xffff,current[1]&0xffff,missing,(unsigned long long)needed);
  if(!GuLogReady() || Gu.fault)return EFI_DEVICE_ERROR;
  /* Never use EFI_PCI_DEVICE_ENABLE: it would also enable bus mastering. */
  EFI_STATUS enabled=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,needed,NULL);
  GuLog("decode enable status=%#llx\n",(unsigned long long)enabled);
  if(!GuLogReady() || Gu.fault)return EFI_ERROR(enabled)?enabled:EFI_DEVICE_ERROR;
  /* Even an error may follow a partial hardware effect. Capture it if readable,
     but never turn an API error into success merely because readback matches. */
  status=ReadConfig(after);
  BOOLEAN matched=FALSE;
  if(!EFI_ERROR(status)) {
    GuLogText("config after decode restore (256 valid bytes):\n");ConfigLog(after);
    matched=!memcmp(after,InitialConfig,sizeof(after));GuLog("decode readback matched=%u\n",matched);
  }
  if(EFI_ERROR(enabled))return enabled;
  if(EFI_ERROR(status))return status;
  if(!GuLogReady() || Gu.fault)return EFI_DEVICE_ERROR;
  return matched?EFI_SUCCESS:EFI_COMPROMISED_DATA;
}
#ifdef GU_FE_EXPERIMENTAL
/* The original FE has PCI MSE=0 before any NVIDIA firmware driver binds.
 * Only the exclusively owned UEFI PCI interface may enable MSE. Verify the
 * full config snapshot differs in exactly one command bit afterward.
 */
static EFI_STATUS FeEnableDecode(VOID) {
  if(!FeMode)return EFI_SUCCESS;
  if(!Gu.owned || Gu.armed || Gu.started || Gu.allocations || Gu.retained || Gu.fault)
    return EFI_ACCESS_DENIED;
  UINT32 before[64],after[64],expected[64];
  EFI_STATUS st=ReadConfig(before);if(EFI_ERROR(st))return st;
  if(memcmp(before,InitialConfig,sizeof(before)))return EFI_COMPROMISED_DATA;
  if(before[1]&4U)return EFI_ACCESS_DENIED;
  if(before[1]&2U)return EFI_SUCCESS;
  UINT64 supported=0;
  st=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&supported);
  if(EFI_ERROR(st) || !(supported&EFI_PCI_IO_ATTRIBUTE_MEMORY))return EFI_UNSUPPORTED;
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  /* Mark pending before the API: it can partially succeed even on error. */
  FeDecodeChanged=TRUE;
  st=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,
    EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
  if(EFI_ERROR(st))return st;
  st=ReadConfig(after);if(EFI_ERROR(st))return st;
  memcpy(expected,before,sizeof(expected));expected[1]|=2U;
  if(memcmp(after,expected,sizeof(after)))return EFI_COMPROMISED_DATA;
  memcpy(InitialConfig,after,sizeof(after));
  GuLogText("FE PCI MSE enabled under exclusive ownership, BME remains OFF.\n");
  return EFI_SUCCESS;
}
static EFI_STATUS FeUndoDecodeIfUnstarted(VOID) {
  if(!FeMode || !FeDecodeChanged || Gu.started || Gu.retained)return EFI_SUCCESS;
  if(!Gu.owned || !Gu.pci || Gu.allocations || Gu.dma_count || Gu.master_attributes_pending)
    return EFI_ACCESS_DENIED;
  UINT32 cfg[64];EFI_STATUS st=ReadConfig(cfg);
  if(EFI_ERROR(st) || (cfg[1]&4U))return EFI_DEVICE_ERROR;
  st=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationDisable,
    EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
  if(EFI_ERROR(st))return st;
  st=ReadConfig(cfg);
  UINT32 expected[64];memcpy(expected,InitialConfig,sizeof(expected));
  expected[1]&=~2U;
  if(EFI_ERROR(st) || memcmp(cfg,expected,sizeof(expected)))
    return EFI_COMPROMISED_DATA;
  FeDecodeChanged=FALSE;
  GuLogText("FE PCI MSE restored to initial OFF state; no firmware attempt began.\n");
  return EFI_SUCCESS;
}
#endif
static EFI_STATUS FindTarget(VOID) {
  EFI_HANDLE *handles=NULL;UINTN count=0,found=0;
  EFI_STATUS status=Gu.bs->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&count,&handles);
  GuLog("PCI enumeration: status=%#llx handles=%zu\n",(unsigned long long)status,(size_t)count);
  if(EFI_ERROR(status) || !handles || !count || count>256){if(handles)GuFree(handles);return EFI_NOT_FOUND;}
  for(UINTN i=0;i<count;i++) {
    EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 id=0;
    status=Gu.bs->HandleProtocol(handles[i],&PciGuid,(VOID **)&pci);
    if(!EFI_ERROR(status) && pci)status=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,1,&id);
    if(EFI_ERROR(status) || !pci){GuLog("PCI scan failed: index=%zu status=%#llx\n",(size_t)i,(unsigned long long)status);status=EFI_DEVICE_ERROR;break;}
    if(id==GU_BOARD_ID){found++;Gu.handle=handles[i];Gu.pci=pci;}
  }
  GuFree(handles);GuLog("PCI target matches=%zu status=%#llx\n",(size_t)found,(unsigned long long)status);
  if(EFI_ERROR(status) || found!=1)return EFI_UNSUPPORTED;
#ifdef GU_FE_EXPERIMENTAL
  UINTN seg,bus,dev,fun;UINT32 cfg[64];
  if(EFI_ERROR(Gu.pci->GetLocation(Gu.pci,&seg,&bus,&dev,&fun)) ||
     EFI_ERROR(ReadConfig(cfg)))return EFI_DEVICE_ERROR;
  FeMode=GuIdentifyProfile(cfg,seg,bus,dev,fun)==GU_PROFILE_FE_OBSERVED;
  if(!FeMode)GuLogText("EXPERIMENTAL build: non-FE cards retain original MSI profile.\n");
#endif
  return ConfigCheck(TRUE);
}
static EFI_STATUS BarCheck(VOID) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=NULL;
  EFI_STATUS status=Gu.pci->GetBarAttributes(Gu.pci,0,NULL,(VOID **)&r);
  GuLog("bar0 query: status=%#llx\n",(unsigned long long)status);
  if(!EFI_ERROR(status)) {
#ifdef GU_FE_EXPERIMENTAL
    if(FeMode) {
      UINT32 cfg[64];EFI_STATUS read=ReadConfig(cfg);
      GU_FE_BAR_OBSERVATION o=GuFeObserveBar0(cfg,r);
      if(EFI_ERROR(read) || !o.descriptor_matches ||
         !o.memory_decode || !o.bar0_base || !r)status=EFI_UNSUPPORTED;
      else {Gu.bar_base=o.bar0_base;Gu.bar_size=r->AddrLen;
        GuLog("FE runtime BAR0: base=%#llx size=%#llx\n",
          (unsigned long long)Gu.bar_base,(unsigned long long)Gu.bar_size);}
      if(r)GuFree(r);
      return status;
    }
#endif
    if(r && r->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && r->Len==sizeof(*r)-3)
      GuLog("bar0 descriptor: type=%u granularity=%llu min=%#llx max=%#llx length=%#llx translation=%#llx\n",
        (unsigned)r->ResType,(unsigned long long)r->AddrSpaceGranularity,(unsigned long long)r->AddrRangeMin,
        (unsigned long long)r->AddrRangeMax,(unsigned long long)r->AddrLen,(unsigned long long)r->AddrTranslationOffset);
    if(!r || r->Desc!=ACPI_ADDRESS_SPACE_DESCRIPTOR || r->Len!=sizeof(*r)-3 || r->ResType!=0 ||
      r->AddrRangeMin!=0x84000000 || r->AddrLen!=0x1000000 || r->AddrTranslationOffset ||
      /* UEFI describes an inclusive end; pinned EDK2 PciBusDxe instead returns
         PciBar.Alignment. Both representations describe this exact 16 MiB BAR. */
      (r->AddrRangeMax!=0x84ffffff && r->AddrRangeMax!=0x00ffffff) ||
      r->AddrSpaceGranularity!=32 || ((UINT8 *)(r+1))[0]!=ACPI_END_TAG_DESCRIPTOR)status=EFI_UNSUPPORTED;
    else {
      UINT8 *raw=(UINT8 *)r,sum=0;
      for(UINTN i=0;i<sizeof(*r)+2;i++)sum=(UINT8)(sum+raw[i]);
      if(raw[sizeof(*r)+1] && sum)status=EFI_COMPROMISED_DATA;
      else {Gu.bar_base=r->AddrRangeMin;Gu.bar_size=r->AddrLen;}
    }
  }
  if(r)GuFree(r);return status;
}
static EFI_STATUS ColdCheck(VOID) {
#ifdef GU_FE_EXPERIMENTAL
  if(FeMode) {
    GU_FE_COLD_SAMPLE s={GuRead32(0),GuRead32(0x118128U),GuRead32(0x118234U),
      GuRead32(0x110100U),GuRead32(0x1fa828U),GuRead32(0x823804U),
      GuRead32(0x82381cU),GuRead32(0x823820U),GuRead32(0x1183a4U),
      GuRead32(0x824148U),GuRead32(0x8241e0U)};
    GU_FE_COLD_ASSESSMENT a=GuFeAssessCold(&s);
    GuLog("FE experiment cold: known=%u gfw_idle=%u fuse=%u SS0=%#x SS1=%#x\n",
      (unsigned)a.known_observation,(unsigned)a.gfw_idle,
      (unsigned)a.signature_slots_supported,s.ss0,s.ss1);
    return !Gu.fault && a.known_observation && a.gfw_idle && a.signature_slots_supported?
      EFI_SUCCESS:EFI_NOT_READY;
  }
#endif
  UINT32 chip=GuRead32(0);GuLog("chip: actual=%#x expected=%#x\n",chip,GU_BOARD_CHIP);
  if(chip!=GU_BOARD_CHIP)return EFI_UNSUPPORTED;
  struct gpu_unlock_gfw_sample s={.config_ok=true,.plm=GuRead32(GU_GFW_PLM_OFFSET),
    .boot=GuRead32(GU_GFW_BOOT_OFFSET),.cpuctl=GuRead32(GU_GFW_CPUCTL_OFFSET)};
  UINT32 hi=GuRead32(0x1fa828),plm=GuRead32(GU_FEAT_PLM_OFFSET),ss0=GuRead32(GU_FEAT_SS0_OFFSET),ss1=GuRead32(GU_FEAT_SS1_OFFSET);
  GuLog("cold state: chip=%#x plm=%#x gfw=%#x/%#x/%#x WPR2_HI=%#x SS0=%#x SS1=%#x\n",
    GU_BOARD_CHIP,plm,s.plm,s.boot,s.cpuctl,hi,ss0,ss1);
  if(Gu.fault || gpu_unlock_gfw_assess(&s) || hi>>4 || plm!=0xffffff8f ||
    ss0!=0x46254040 || (ss1!=2 && ss1!=3) || GuRead32(0x1183a4)!=0x2800)return EFI_NOT_READY;
  return Gu.fault?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS PromCheck(VOID) {
  VOID *prom=GuAlloc(1048576);if(!prom)return EFI_OUT_OF_RESOURCES;
  GuCopyFromIo(prom,(VOID *)(UINTN)(Gu.bar_base+0x300000),1048576);
#ifdef GU_FE_EXPERIMENTAL
  if(FeMode) {
    BOOLEAN matched=!Gu.fault && GuHashMatches(prom,1048576,
      "d2a902d96f78783c42e39c94bb241e3644f386196b35b9e0c7d0820945e5735d");
    struct gpu_unlock_fwsec fw={0};
    int parsed=matched?gpu_unlock_vbios_fwsec(prom,1048576,&fw):-1;
    BOOLEAN shape=parsed==0 && fw.image_off==0x4239cU && fw.image_size==0xde00U &&
      fw.dmem_size==0x800U && fw.ucode_id==9U && fw.engine_mask==0x400U &&
      fw.sig_versions==7U && fw.sig_count==3U && fw.sig_bytes==1152U;
    GuLog("FE experiment live PROM: sha=%u FWSEC_shape=%u\n",
      (unsigned)matched,(unsigned)shape);
    GuFree(prom);
    return matched && shape?EFI_SUCCESS:EFI_CRC_ERROR;
  }
#endif
  BOOLEAN good=!Gu.fault && GuHashMatches(prom,1048576,"f3dc8f07b0b8c9856d192df172dfa7f7f84ac17ef6edfdb448ad7fa94efd2789");
  GuFree(prom);GuLog("live PROM hash matched=%u\n",(unsigned)good);return good?EFI_SUCCESS:EFI_CRC_ERROR;
}
static EFI_STATUS Arm(VOID) {
#ifdef GU_FE_HEADLESS
  GuLogText("STAGE19 HEADLESS: Stage15 one-shot marker consumed; attempting once.\n");
  return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
#endif
  if(!Table->ConIn || !Table->ConIn->Reset || !Table->ConIn->ReadKeyStroke)return EFI_UNSUPPORTED;
  EFI_STATUS status=Table->ConIn->Reset(Table->ConIn,FALSE);if(EFI_ERROR(status))return status;
#ifdef GU_FE_EXPERIMENTAL
  if(FeMode)
    GuLogText("DANGER: EXPERIMENTAL RTX 3080 FE! U launches real DMA/FWSEC/SEC2/FLR. GPU could hang. Power-off recovery may be required.\n");
  else
#endif
  GuLogText("READY: MSI RTX 3080 10GB / Intel display / verified USB log and firmware.\n");
  if(PendingOwner.agent)GuLogText("U will release the RTX firmware driver, restore verified access settings if needed, then recheck and attempt once.\n");
  GuLogText("Press U to attempt once; Esc or 30-second timeout cancels.\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  for(UINTN i=0;i<300;i++) {
    EFI_INPUT_KEY key;status=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);
    if(!EFI_ERROR(status)) {
      if(key.ScanCode==0x17 || key.UnicodeChar==27)return EFI_ABORTED;
      if(key.UnicodeChar=='u' || key.UnicodeChar=='U')return EFI_SUCCESS;
    } else if(status!=EFI_NOT_READY)return status;
    GuStall(100000);
  }
  return EFI_TIMEOUT;
}
static VOID Halt(VOID) {
  Gu.retained=TRUE;
  GuLogText("STOP: cleanup cannot be proven. Resources remain reserved.\nPOWER OFF using the physical power button; do not continue to Windows.\n");
  /* No ResetSystem, ExitBootServices, retry, protocol close or memory release. */
  for(;;)Gu.bs->Stall(1000000);
}
static VOID LogContainment(VOID) {
  /* Called only after any TPL_NOTIFY containment section has ended. */
  GuLog("BME containment: attribute_status=%#llx hardware_status=%#llx command=%#x cached=%#llx pending=%u\n",
    (unsigned long long)Gu.master_attribute_status,(unsigned long long)Gu.master_hardware_status,
    Gu.master_last_command,(unsigned long long)Gu.master_last_attributes,Gu.master_attributes_pending);
}
static VOID ReleaseHalt(EFI_STATUS Status,BOOLEAN Attempted) {
  /* A failed claim may follow interface removal/replacement during logging.
     Resolve, validate and clear BME without a callback window or stale access. */
  EFI_TPL old=Gu.bs->RaiseTPL(TPL_NOTIFY);
  EFI_STATUS refreshed=GuRefreshTargetPci();
  EFI_STATUS contained=EFI_ERROR(refreshed)?refreshed:GuClearMaster();
  Gu.bs->RestoreTPL(old);
  LogContainment();
  GuLog("FINAL status=%#llx stage=%s attempted=%u driver_release=1 clean=0 containment=%#llx pci_available=%u\n",
    (unsigned long long)Status,Stage,Attempted,(unsigned long long)contained,(unsigned)(Gu.pci!=NULL));
  Halt();
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
  if(!St || !St->BootServices || !St->RuntimeServices)return EFI_INVALID_PARAMETER;
  Table=St;GuInit(St->BootServices,NULL);Gu.image=Image;Gu.display_check=DisplayCheck;
  memset(&PendingOwner,0,sizeof(PendingOwner));
  Stage="usb-log";EFI_STATUS status=GuIoOpen(Image,St);BOOLEAN attempted=FALSE,release_attempted=FALSE;
  if(EFI_ERROR(status))goto finish;
  UINT8 secure=0xff;UINTN bytes=1;
  Stage="secure-boot";
  status=St->RuntimeServices->GetVariable(L"SecureBoot",&GlobalGuid,NULL,&bytes,&secure);
  if(EFI_ERROR(status) || bytes!=1 || secure){status=EFI_SECURITY_VIOLATION;goto finish;}
  status=Step(status);if(EFI_ERROR(status))goto finish;
  Stage="watchdog";status=Step(Gu.bs->SetWatchdogTimer(0,0,0,NULL));if(EFI_ERROR(status))goto finish;
  Stage="target";status=Step(FindTarget());if(EFI_ERROR(status))goto finish;
  Stage="display";Gu.display_ok=DisplayCheck();status=Step(Gu.display_ok?EFI_SUCCESS:EFI_UNSUPPORTED);if(EFI_ERROR(status))goto finish;
  Stage="ownership";status=GuClaimController();
  if(status==EFI_ACCESS_DENIED) {
    status=GuFindReleaseOwner(St,&PendingOwner,TRUE);
    if(!EFI_ERROR(status))GuLogText("Exclusive ownership deferred until manual U and explicit driver release.\n");
  }
  status=Step(status);if(EFI_ERROR(status))goto finish;
#ifdef GU_FE_EXPERIMENTAL
  Stage="fe-enable-mse";status=Step(FeEnableDecode());if(EFI_ERROR(status))goto finish;
#endif
  Stage="config-recheck";status=Step(ConfigCheck(FALSE));if(EFI_ERROR(status))goto finish;
  Stage="bar0";status=Step(BarCheck());if(EFI_ERROR(status))goto finish;
  Stage="cold-state";status=Step(ColdCheck());if(EFI_ERROR(status))goto finish;
  Stage="prom";status=Step(PromCheck());if(EFI_ERROR(status))goto finish;
  Stage="assets";status=Step(GuLoadAssets());if(EFI_ERROR(status))goto finish;
  Stage="arming";status=Step(Arm());if(EFI_ERROR(status))goto finish;
  Stage="armed-config";status=Step(ConfigCheck(FALSE));if(EFI_ERROR(status))goto finish;
  Stage="armed-display";Gu.display_ok=DisplayCheck();status=Step(Gu.display_ok?EFI_SUCCESS:EFI_UNSUPPORTED);if(EFI_ERROR(status))goto finish;
  Stage="armed-cold-state";status=Step(ColdCheck());if(EFI_ERROR(status))goto finish;
  if(PendingOwner.agent) {
    GU_RELEASE_OWNER current;
    Stage="owner-recheck";
    GuLogText("Manual U confirmed: release RTX firmware driver once, then revalidate.\n");
    if(!GuLogReady()){status=EFI_DEVICE_ERROR;goto finish;}
    /* Durable intent precedes the last owner snapshot. No file IO between a
       successful final comparison and DisconnectController. */
    status=GuFindReleaseOwner(St,&current,FALSE);
    if(!EFI_ERROR(status) && !GuSameReleaseOwner(&PendingOwner,&current))status=EFI_ABORTED;
    if(EFI_ERROR(status)){status=Step(status);goto finish;}
    EFI_PCI_IO_PROTOCOL *prior=Gu.pci;
    Stage="driver-release";release_attempted=TRUE;Gu.armed=TRUE;Gu.pci=NULL;
    EFI_STATUS released=Gu.bs->DisconnectController(Gu.handle,current.agent,NULL);
    EFI_TPL refresh_tpl=Gu.bs->RaiseTPL(TPL_NOTIFY);
    EFI_STATUS refreshed=GuRefreshTargetPci();
    Gu.bs->RestoreTPL(refresh_tpl);
    GuLog("driver release: status=%#llx refresh=%#llx same_pci=%u\n",
      (unsigned long long)released,(unsigned long long)refreshed,(unsigned)(Gu.pci==prior));
    status=Step(EFI_ERROR(released)?released:EFI_ERROR(refreshed)?refreshed:Gu.pci!=prior?EFI_COMPROMISED_DATA:EFI_SUCCESS);
    if(EFI_ERROR(status))goto finish;
    Stage="ownership-after-release";status=Step(GuClaimController());if(EFI_ERROR(status))goto finish;
    Stage="display-after-release";Gu.display_ok=DisplayCheck();status=Step(Gu.display_ok?EFI_SUCCESS:EFI_UNSUPPORTED);if(EFI_ERROR(status))goto finish;
    Stage="decode-after-release";status=Step(RestoreDecode());if(EFI_ERROR(status))goto finish;
    Stage="config-after-release";status=Step(ConfigCheck(FALSE));if(EFI_ERROR(status))goto finish;
    Stage="bar0-after-release";status=Step(BarCheck());if(EFI_ERROR(status))goto finish;
    Stage="cold-after-release";status=Step(ColdCheck());if(EFI_ERROR(status))goto finish;
    Stage="prom-after-release";status=Step(PromCheck());if(EFI_ERROR(status))goto finish;
  }
  GuLogText("ARMED: one transaction is starting. Do not reset or remove the USB.\n");
  if(!GuLogReady()){status=EFI_DEVICE_ERROR;goto finish;}
  Stage="transaction";Gu.armed=TRUE;attempted=TRUE;
  UINT64 mask=0xffffffffULL;
  struct pci_dev device={.dev={.dma_mask=&mask,.coherent_dma_mask=0xffffffffULL},.pm_cap=Gu.pm_cap,.refs=1};
  struct gpu_unlock_610_shared shared={0};
  int result=gpu_unlock_610_load_shared(&device,&shared);
  if(!result)result=gpu_unlock_610_unlock(&device,&shared);
  /* Also covers failures before GuStarted, including BME-enable readback. */
  EFI_STATUS contained=GuClearMaster();
  LogContainment();
  GuLog("transaction result=%d containment=%#llx started=%u quiesced=%u fault=%u retained=%u maps=%u FLR=%u\n",
    result,(unsigned long long)contained,Gu.started,Gu.quiesced,Gu.fault,Gu.retained,Gu.dma_count,Gu.flrs);
  /* An experimental FE failure after ANY MMIO write cannot be handed to an
   * untested Windows boot path. Contain the GPU and require full power-off.
   */
#ifdef GU_FE_EXPERIMENTAL
  if(FeMode && result && Gu.mmio_writes) {
    GuLogText("FE GPU state changed before failure; Windows handoff refused.\n");
    Halt();
  }
#endif
  if(EFI_ERROR(contained) || !GuSafeToReturn() || Gu.allocations || (Gu.started && result))Halt();
  gpu_unlock_610_release_shared(&shared);Gu.armed=FALSE;
  status=result?EFI_DEVICE_ERROR:EFI_SUCCESS;
finish:
#ifdef GU_FE_EXPERIMENTAL
  /* Cancellation, or a clean pre-write failure, restores FE's initial MSE=0.
   * After a GPU register write we NEVER try a speculative restoration.
   */
  if(FeMode && FeDecodeChanged && !Gu.mmio_writes && !Gu.started && !Gu.retained) {
    EFI_STATUS rest=FeUndoDecodeIfUnstarted();
    GuLog("FE decode restoration on cancellation: %#llx\n",(unsigned long long)rest);
    if(EFI_ERROR(rest)) {
      GuLogText("FE PCI decode restore failed; do not continue to OS.\n");Halt();
    }
  }
#endif
  if(release_attempted && (EFI_ERROR(status) || Gu.fault))ReleaseHalt(status,attempted);
  if(attempted && !GuSafeToReturn())Halt();
  if(GuLogReady())GuLog("FINAL status=%#llx stage=%s attempted=%u driver_release=%u clean=%u\n",(unsigned long long)status,Stage,attempted,release_attempted,GuSafeToReturn());
  else GuConsole("Preflight or logging failed; no new attempt will start.\n");
  if(!GuLogReady()){status=EFI_DEVICE_ERROR;if(release_attempted)ReleaseHalt(status,attempted);if(Gu.started)Halt();}
  if(attempted && !GuSafeToReturn())Halt();
  EFI_STATUS closed=GuIoClose();
  if(EFI_ERROR(closed)){if(release_attempted)ReleaseHalt(closed,attempted);if(Gu.started)Halt();status=closed;}
  if(Gu.owned) {
    closed=Gu.bs->CloseProtocol(Gu.handle,&PciGuid,Image,NULL);
    if(EFI_ERROR(closed)){if(release_attempted)ReleaseHalt(closed,attempted);if(Gu.started)Halt();status=closed;}else Gu.owned=FALSE;
  }
  GuReleaseAssets();
  if(!EFI_ERROR(status))GuConsole("UEFI transaction completed and cleaned up. Returning to firmware; Windows validation is pending.\n");
  else GuConsole("Attempt cancelled or rejected. Returning without another attempt.\n");
  return status;
}
