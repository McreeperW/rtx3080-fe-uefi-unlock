#pragma once
/*
 * RTX 3080 board inventory; never an authorization shortcut.
 * These values identify observed hardware only. MSI approval still requires
 * all original PCI/config, GOP, PROM, cold-state, and asset checks.
 * A Founders Edition is ALWAYS blocked from the transaction in this version.
 */
#include "port.h"

typedef enum {
  GU_PROFILE_UNKNOWN = 0,
  GU_PROFILE_MSI_VALIDATED = 1,
  GU_PROFILE_FE_OBSERVED = 2
} GU_PROFILE_KIND;

static inline GU_PROFILE_KIND GuIdentifyProfile(const UINT32 Config[64],
    UINTN Segment, UINTN Bus, UINTN Device, UINTN Function) {
  if (!Config || Segment || Device || Function || Config[0] != GU_BOARD_ID ||
      (Config[2] >> 24) != 0x03U || ((Config[3] >> 16) & 0x7fU))
    return GU_PROFILE_UNKNOWN;
  if (Bus == 1 && Config[0x2c / 4] == GU_BOARD_SUBSYSTEM)
    return GU_PROFILE_MSI_VALIDATED;
  /* FE bus number and BAR0 base are runtime observations, not stable IDs. */
  if (Config[0x2c / 4] == 0x146710deU)
    return GU_PROFILE_FE_OBSERVED;
  return GU_PROFILE_UNKNOWN;
}

static inline BOOLEAN GuProfileAllowsOriginalUnlock(GU_PROFILE_KIND Kind) {
  /* Do not broaden this allowlist without independent FE validation. */
  return Kind == GU_PROFILE_MSI_VALIDATED;
}

static inline const char *GuProfileName(GU_PROFILE_KIND Kind) {
  switch (Kind) {
    case GU_PROFILE_MSI_VALIDATED: return "msi-pinned";
    case GU_PROFILE_FE_OBSERVED: return "fe-observed-only";
    default: return "unknown";
  }
}

static inline UINT32 GuProfileBar0Base(const UINT32 Config[64]) {
  UINT32 Bar;
  if (!Config) return 0;
  Bar = Config[0x10 / 4];
  /* 32-bit memory BAR only. Reject I/O or 64-bit layouts; no probing writes. */
  return ((Bar & 1U) == 0 && ((Bar >> 1U) & 3U) == 0) ?
    (Bar & 0xfffffff0U) : 0;
}

static inline BOOLEAN GuProfileMemoryDecodeEnabled(const UINT32 Config[64]) {
  return Config && (Config[0x04 / 4] & 2U) != 0;
}
