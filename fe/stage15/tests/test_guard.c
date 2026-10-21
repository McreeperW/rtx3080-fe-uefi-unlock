#include "../src/guard.h"
#include <assert.h>
#include <stdio.h>
static S14Observation base(void) {
  S14Observation o={0};
  o.matched_count=1;o.location_ok=1;o.pci_data_ok=1;
  o.device_id=0x220610deu;o.subsystem_id=0x146710deu;
  o.pci_owner_query_ok=1;o.gop_owner_query_ok=1;
  return o;
}
int main(void) {
  S14Observation o=base();
  /* Real 2026-10-21 Ryzen 3600 log: command 0x7, active GOP, BY_DRIVER. */
  o.pci_command=7;o.gop_routes=1;o.pci_bound_driver=1;o.gop_bound_driver=1;
  uint32_t m=s14_gate(&o);
  assert((m&(S14_PCI_IO_ACTIVE|S14_MEMORY_DECODE_ACTIVE|S14_BUS_MASTER_ACTIVE|S14_ACTIVE_NVIDIA_GOP|S14_DRIVER_BOUND|S14_NO_RESET_RECOVERY_PROOF|S14_STAGE12_NOT_VERIFIED))==
         (S14_PCI_IO_ACTIVE|S14_MEMORY_DECODE_ACTIVE|S14_BUS_MASTER_ACTIVE|S14_ACTIVE_NVIDIA_GOP|S14_DRIVER_BOUND|S14_NO_RESET_RECOVERY_PROOF|S14_STAGE12_NOT_VERIFIED));
  puts("PASS: current hardware blocked");
  o=base();m=s14_gate(&o);
  assert(m==(S14_NO_RESET_RECOVERY_PROOF|S14_STAGE12_NOT_VERIFIED));
  puts("PASS: quiescent GPU still blocked without recovery and verified Stage12");
  o=base();o.pci_owner_query_ok=0;assert(s14_gate(&o)&S14_OWNER_QUERY_INCOMPLETE);
  puts("PASS: unknown ownership blocked");
  o=base();o.gop_routes=1;assert(s14_gate(&o)&S14_ACTIVE_NVIDIA_GOP);
  puts("PASS: GOP ownership blocked");
  o=base();o.recovery_proven=1;o.stage12_integrated_and_verified=1;
  assert(s14_gate(&o)==0);puts("PASS: pure guard model accepts fully validated synthetic case");
  return 0;
}
