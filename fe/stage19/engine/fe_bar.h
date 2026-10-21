#pragma once
/*
 * Founders Edition PCI BAR0 inventory only. No MMIO or configuration writes.
 * This helper never grants permission to run the MSI unlock transaction.
 */
#include "profiles.h"
#include <IndustryStandard/Acpi.h>

typedef struct {
  UINT16 command;
  UINT32 bar0_raw;
  UINT32 bar0_base;
  BOOLEAN memory_decode;
  BOOLEAN descriptor_matches;
} GU_FE_BAR_OBSERVATION;

static inline GU_FE_BAR_OBSERVATION GuFeObserveBar0(
    const UINT32 Config[64], const EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Bar) {
  GU_FE_BAR_OBSERVATION Out = {0, 0, 0, FALSE, FALSE};
  if (!Config) return Out;
  Out.command = (UINT16)(Config[1] & 0xffffU);
  Out.bar0_raw = Config[4];
  Out.bar0_base = GuProfileBar0Base(Config);
  Out.memory_decode = GuProfileMemoryDecodeEnabled(Config);
  if (!Out.bar0_base || !Bar) return Out;
  /* A validated descriptor proves a reported PCI BAR resource, not usable MMIO. */
  if (Bar->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR ||
      Bar->Len != sizeof(*Bar) - 3U || Bar->ResType != 0 ||
      Bar->AddrSpaceGranularity != 32 || Bar->AddrTranslationOffset != 0 ||
      Bar->AddrRangeMin != Out.bar0_base || Bar->AddrLen != 0x1000000ULL)
    return Out;
  /* EDK2 PciBusDxe may report either the inclusive end or alignment mask. */
  if (Bar->AddrRangeMax != Bar->AddrRangeMin + Bar->AddrLen - 1ULL &&
      Bar->AddrRangeMax != Bar->AddrLen - 1ULL)
    return Out;
  Out.descriptor_matches = TRUE;
  return Out;
}
