#include "guard.h"
uint32_t s14_gate(const S14Observation *o) {
  if(!o)return 0xFFFFFFFFu;
  uint32_t deny=0;
  if(o->matched_count!=1)deny|=S14_NOT_UNIQUE;
  if(!o->pci_data_ok || !o->location_ok ||
     o->device_id!=0x220610deu || o->subsystem_id!=0x146710deu)
    deny|=S14_IDENTITY_MISMATCH;
  if(o->pci_command&1u)deny|=S14_PCI_IO_ACTIVE;
  if(o->pci_command&2u)deny|=S14_MEMORY_DECODE_ACTIVE;
  if(o->pci_command&4u)deny|=S14_BUS_MASTER_ACTIVE;
  if(o->gop_routes)deny|=S14_ACTIVE_NVIDIA_GOP;
  if(!o->pci_owner_query_ok || !o->gop_owner_query_ok)
    deny|=S14_OWNER_QUERY_INCOMPLETE;
  if(o->pci_bound_driver || o->gop_bound_driver)deny|=S14_DRIVER_BOUND;
  if(o->pci_child_controller || o->gop_child_controller)
    deny|=S14_CHILD_CONTROLLER_BOUND;
  if(!o->recovery_proven)deny|=S14_NO_RESET_RECOVERY_PROOF;
  if(!o->stage12_integrated_and_verified)deny|=S14_STAGE12_NOT_VERIFIED;
  return deny;
}
