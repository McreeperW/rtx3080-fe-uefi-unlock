from pathlib import Path
p=Path(__file__).resolve().parent
s=p/'src'/'bridge.c'
a=s.read_text()
a=a.replace('RTX3080 FE Stage18 single-GPU released-state snapshot experiment for Stage15.','RTX3080 FE Stage19 single-GPU experimental headless unlock bridge.')
a=a.replace('NO GPU PCI/MMIO writes, DMA, FLR, unlock or firmware updates.','EXPERIMENTAL: launches Stage12 firmware unlock with GPU DMA/FLR; do not repeat automatically.')
a=a.replace('L"\\\\RTX3080-STAGE18-SNAPSHOT.TXT"','L"\\\\RTX3080-STAGE19-UNLOCK.TXT"')
# source file used L"\\RTX3080.." (one C backslash) Python inspect
if 'STAGE18-SNAPSHOT.TXT' in a:a=a.replace('RTX3080-STAGE18-SNAPSHOT.TXT','RTX3080-STAGE19-UNLOCK.TXT')
a=a.replace('RTX3080 FE STAGE18 RELEASED-STATE PCI/MMIO SNAPSHOT / RECONNECT','RTX3080 FE STAGE19 EXPERIMENTAL HEADLESS UNLOCK / RECOVERY')
a=a.replace('NO PCI/MMIO WRITES; no DMA/FLR/unlock. UEFI driver disconnect/reconnect occurs.','EXPERIMENTAL GPU DMA/SEC2/GSP/FLR MAY HANG. Stage15 Windows recovery only if this image returns.')
# import USB LoadImage helper from stable Stage15 C
stable=Path('/mnt/data/stage15pkg/fe/stage15/src/bootbridge.c').read_text()
start=stable.index('static EFI_STATUS load_usb_child(')
end=stable.index('static void run_child_once(',start)
helper=stable[start:end].replace('load_usb_child','load_stage19_engine').replace('L"\\\\EFI\\\\BOOT\\\\RTX3080-UNLOCK.EFI"','L"\\\\EFI\\\\BOOT\\\\RTX3080-STAGE19-ENGINE.EFI"')
assert 'RTX3080-STAGE19-ENGINE.EFI' in helper
# All structs from EFI standard; no global LoadedGuid name collision.
marker='static EFI_STATUS log_flush(void){return Log?Log->Flush(Log):EFI_DEVICE_ERROR;}'
assert marker in a
a=a.replace(marker,helper+'\n'+marker)
# add flags to efi_main
needle='  BOOLEAN reconnect_verified=FALSE;'
assert needle in a
a=a.replace(needle,needle+'\n  BOOLEAN unlock_verified=FALSE, engine_returned=FALSE, engine_loaded=FALSE;\n  EFI_STATUS engine_result=EFI_NOT_READY;')
# key stage18 main replace block
start=a.index('  /* Read post-detach state while the GPU graphics driver is disconnected. */')
end=a.index('\n  /* Reconnect even if disconnect returned error:',start)
replacement=r'''  /* Stage18 evidence: PCI command is 0 after driver release.  With no
     driver and no BME, enable ONLY memory decode for the Stage12 engine. */
  EFI_PCI_IO_PROTOCOL *pci=NULL;
  UINT32 release_cmd=0xffffffffU;
  BOOLEAN decode_enabled=FALSE;
  if(!EFI_ERROR(disconnect_status)&&!EFI_ERROR(owner_rc)&&released_bits==0U){
    output("stage19_snapshot_phase=RELEASED\n");
    inspect_pci_state(gpu,"stage19_released_");
    EFI_STATUS ps=ST->BootServices->HandleProtocol(gpu,&PciGuid,(VOID**)&pci);
    if(!EFI_ERROR(ps)&&pci)ps=pci->Pci.Read(pci,EfiPciIoWidthUint32,4,1,&release_cmd);
    ls("stage19_released_command_status=",ps);
    ln("stage19_released_command=",release_cmd&0xffffU);
    UINT64 support=0;
    if(!EFI_ERROR(ps)&&release_cmd==0x00100000U){ /* PCI status high word is not fixed; use low word instead */
      output("stage19_note=UNUSED_MASK_CONDITION\n");
    }
    if(!EFI_ERROR(ps)&& (release_cmd&0xffffU)==0U &&pci &&pci->Attributes){
      ps=pci->Attributes(pci,EfiPciIoAttributeOperationSupported,0,&support);
      ls("stage19_attribute_supported_status=",ps);
      ln("stage19_attribute_supported=",support);
      if(!EFI_ERROR(ps)&&(support&EFI_PCI_IO_ATTRIBUTE_MEMORY)){
        if(!EFI_ERROR(log_flush())){
          ps=pci->Attributes(pci,EfiPciIoAttributeOperationEnable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
          ls("stage19_enable_memory_status=",ps);
          UINT32 after=0xffffffffU;
          if(!EFI_ERROR(ps))ps=pci->Pci.Read(pci,EfiPciIoWidthUint32,4,1,&after);
          ln("stage19_after_enable_command=",after&0xffffU);
          if(!EFI_ERROR(ps)&&(after&0xffffU)==2U)decode_enabled=TRUE;
        }
      }
    }
    if(decode_enabled){
      inspect_pci_state(gpu,"stage19_pre_engine_");
      EFI_HANDLE child=NULL;
      if(!EFI_ERROR(log_flush())){
        EFI_STATUS load=load_stage19_engine(image,&child);
        ls("stage19_engine_load_status=",load);
        if(!EFI_ERROR(load)&&child){
          engine_loaded=TRUE;
          (void)log_flush();
          engine_result=ST->BootServices->StartImage(child,NULL,NULL);
          engine_returned=TRUE;
          ls("stage19_engine_return_status=",engine_result);
          /* A success return from Stage12 is insufficient; read selectors. */
          if(!EFI_ERROR(engine_result)){
            UINT32 ss0=0xffffffffU,ss1=0xffffffffU;
            EFI_STATUS r0=pci->Mem.Read(pci,EfiPciIoWidthUint32,0,0x82381cU,1,&ss0);
            EFI_STATUS r1=pci->Mem.Read(pci,EfiPciIoWidthUint32,0,0x823820U,1,&ss1);
            ls("stage19_post_engine_ss0_status=",r0);
            ls("stage19_post_engine_ss1_status=",r1);
            ln("stage19_post_engine_ss0=",ss0);
            ln("stage19_post_engine_ss1=",ss1);
            unlock_verified=!EFI_ERROR(r0)&&!EFI_ERROR(r1)&&ss0==0x88888888U&&ss1==8U;
          }
          (void)ST->BootServices->UnloadImage(child);
        }
      }
    }else output("stage19_result=SKIP_DECODE_NOT_VERIFIED\n");
    /* Regardless of returned error, restore PCI decode to the Stage18
       detached state before reconnect. If engine hung, this code is unreachable. */
    if(pci&&pci->Attributes){
      EFI_STATUS ds=pci->Attributes(pci,EfiPciIoAttributeOperationDisable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
      ls("stage19_decode_disable_status=",ds);
      UINT32 restored=0xffffffffU;
      EFI_STATUS vr=pci->Pci.Read(pci,EfiPciIoWidthUint32,4,1,&restored);
      ls("stage19_released_restore_command_status=",vr);
      ln("stage19_released_restore_command=",restored&0xffffU);
      if(EFI_ERROR(ds)||EFI_ERROR(vr)||(restored&7U))unlock_verified=FALSE;
    }
  }else output("stage19_result=SKIP_DRIVER_RELEASE_FAILED\n");
  (void)log_flush();'''
a=a[:start]+replacement+a[end:]
# remove misleading unused mask check
st='    if(!EFI_ERROR(ps)&&release_cmd==0x00100000U){ /* PCI status high word is not fixed; use low word instead */\n      output("stage19_note=UNUSED_MASK_CONDITION\\n");\n    }\n'
a=a.replace(st,'')
# modify final success line and exit code
old='''  if(!EFI_ERROR(disconnect_status)&&!EFI_ERROR(owner_rc)&&released_bits==0U&&
      !EFI_ERROR(connect_status)&&!EFI_ERROR(restored_rc)&&
      (restored_bits&3U)==3U&&!EFI_ERROR(gop_rc)&&restored_gops==1&&
      (gop_bits&1U)==1U){
    reconnect_verified=TRUE;
    output("stage18_result=PASS_RELEASE_RECONNECT_SNAPSHOT_NO_UNLOCK\\n");
  } else {
    output("stage18_result=FAIL_OR_PARTIAL_RELEASE_RECONNECT\\n");
  }'''
new='''  if(!EFI_ERROR(disconnect_status)&&!EFI_ERROR(owner_rc)&&released_bits==0U&&
      !EFI_ERROR(connect_status)&&!EFI_ERROR(restored_rc)&&
      (restored_bits&3U)==3U&&!EFI_ERROR(gop_rc)&&restored_gops==1&&
      (gop_bits&1U)==1U){
    reconnect_verified=TRUE;
    output("stage19_reconnect=PASS\\n");
  } else {
    output("stage19_reconnect=FAILED_OR_PARTIAL\\n");
  }'''
assert old in a
a=a.replace(old,new)
old='''  output(reconnect_verified?"stage18_child_return=EFI_SUCCESS_NO_UNLOCK\\n":"stage18_child_return=EFI_ACCESS_DENIED_NO_UNLOCK\\n");
  (void)Log->Flush(Log);(void)Log->Close(Log);Log=NULL;
  /* A successful return only means graphics driver rebinding worked; NEVER implies unlock. */
  return reconnect_verified?EFI_SUCCESS:EFI_ACCESS_DENIED;'''
new='''  ln("stage19_engine_loaded=",engine_loaded);
  ln("stage19_engine_returned=",engine_returned);
  ln("stage19_selector_verify=",unlock_verified);
  if(reconnect_verified && engine_returned && unlock_verified && engine_result==EFI_SUCCESS)
    output("stage19_result=PASS_UNLOCK_SELECTORS_AND_RECONNECT\\n");
  else output("stage19_result=FAILED_OR_SKIPPED_WINDOWS_HANDOFF_IF_RETURNED\\n");
  if(Log){(void)Log->Flush(Log);(void)Log->Close(Log);Log=NULL;}
  return (reconnect_verified && engine_returned && unlock_verified && engine_result==EFI_SUCCESS)?EFI_SUCCESS:EFI_DEVICE_ERROR;'''
assert old in a
a=a.replace(old,new)
s.write_text(a)
print('PASS: shim C prepared')
