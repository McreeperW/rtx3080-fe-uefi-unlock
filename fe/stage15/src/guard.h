#ifndef RTX3080_STAGE14_GUARD_H
#define RTX3080_STAGE14_GUARD_H
#include <stdint.h>
/* Pure, host-testable, fail-closed policy. No GPU I/O in this module. */
typedef struct {
  uint32_t device_id, subsystem_id;
  uint16_t pci_command;
  uint8_t matched_count, location_ok, pci_data_ok;
  uint8_t gop_routes, other_display_count;
  uint8_t pci_owner_query_ok, gop_owner_query_ok;
  uint8_t pci_bound_driver, gop_bound_driver;
  uint8_t pci_child_controller, gop_child_controller;
  uint8_t recovery_proven, stage12_integrated_and_verified;
} S14Observation;
enum {
  S14_NOT_UNIQUE              = 1u << 0,
  S14_IDENTITY_MISMATCH       = 1u << 1,
  S14_PCI_IO_ACTIVE           = 1u << 2,
  S14_MEMORY_DECODE_ACTIVE    = 1u << 3,
  S14_BUS_MASTER_ACTIVE       = 1u << 4,
  S14_ACTIVE_NVIDIA_GOP       = 1u << 5,
  S14_OWNER_QUERY_INCOMPLETE  = 1u << 6,
  S14_DRIVER_BOUND            = 1u << 7,
  S14_CHILD_CONTROLLER_BOUND  = 1u << 8,
  S14_NO_RESET_RECOVERY_PROOF = 1u << 9,
  S14_STAGE12_NOT_VERIFIED    = 1u << 10
};
uint32_t s14_gate(const S14Observation *o);
#endif
