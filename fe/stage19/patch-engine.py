#!/usr/bin/env python3
"""Apply source overlays to an isolated local Stage12 worktree, never overwrite main unlock/."""
from pathlib import Path
p=Path(__file__).resolve().parent
src=p/'engine'
assert (src/'app.c').is_file()
a=(src/'app.c').read_text()
a=a.replace('static BOOLEAN FeDisplayAuditReadOnly(VOID);','static BOOLEAN FeDisplayAuditReadOnly(VOID);\n#ifdef GU_FE_HEADLESS\nstatic BOOLEAN FeHeadlessDisplayCheck(VOID);\n#endif')
needle='  if(FeMode)return FeDisplayAuditReadOnly();'
assert a.count(needle)==1
a=a.replace(needle,'#ifdef GU_FE_HEADLESS\n  if(FeMode)return FeHeadlessDisplayCheck();\n#else\n  if(FeMode)return FeDisplayAuditReadOnly();\n#endif')
# Define a strict, no-GOP, no-driver-owned preflight that runs repeatedly until FLR.
loc='static EFI_STATUS ReadConfig(UINT32 *Config) {'
assert a.count(loc)==1
func=r'''#ifdef GU_FE_HEADLESS
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
'''
a=a.replace(loc,func+loc)
# The headless external child is already bound to Stage15's explicit one-shot marker.
loc='static EFI_STATUS Arm(VOID) {'
assert a.count(loc)==1
a=a.replace(loc,loc+'\n#ifdef GU_FE_HEADLESS\n  GuLogText("STAGE19 HEADLESS: Stage15 one-shot marker consumed; attempting once.\\n");\n  return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;\n#endif')
(src/'app.c').write_text(a)
port=(src/'app_io.c').read_text()
loc='VOID GuConsole(const char *Text) {'
assert port.count(loc)==1
port=port.replace(loc,loc+'\n#ifdef GU_FE_HEADLESS\n  (void)Text; return; /* no ConsoleOut calls after NVIDIA GOP detach */\n#endif')
(src/'app_io.c').write_text(port)
cold=(src/'fe_cold.h').read_text()
needle='(s->ss0==0x10240014U || s->ss0==0x10241014U)'
assert cold.count(needle)==1
cold=cold.replace(needle,'(s->ss0==0x10240014U || s->ss0==0x10241014U || s->ss0==0x10341014U)')
(src/'fe_cold.h').write_text(cold)
print('PASS: headless engine patches applied')
