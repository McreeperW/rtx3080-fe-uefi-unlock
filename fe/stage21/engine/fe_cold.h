#pragma once
/*
 * FE cold-state evidence classification, NOT unlock authorization.
 * Receives already-read GPU register values; NEVER reads/writes hardware.
 * 'Known' means matched a specifically observed 3080 FE cold-state variant,
 * not that GPU firmware execution, resets, DMA, or changed SS0/SS1 are safe.
 */
#include <Uefi.h>

typedef struct {
  UINT32 chip, gfw_plm, gfw_boot, gfw_cpuctl, wpr2_hi;
  UINT32 feature_plm, ss0, ss1, plm_1183a4;
  UINT32 booter_fuse, fwsec_fuse;
} GU_FE_COLD_SAMPLE;

typedef struct {
  BOOLEAN known_observation;
  BOOLEAN gfw_idle;
  BOOLEAN signature_slots_supported;
  BOOLEAN differs_from_msi;
} GU_FE_COLD_ASSESSMENT;

static inline GU_FE_COLD_ASSESSMENT GuFeAssessCold(const GU_FE_COLD_SAMPLE *s) {
  GU_FE_COLD_ASSESSMENT a={FALSE,FALSE,FALSE,FALSE};
  if (!s) return a;
  /* Distinguish a real halted GSP state from disconnected 0xffffffff reads. */
  a.gfw_idle=s->gfw_plm==0x8b8fU && s->gfw_boot==0x3ffU && s->gfw_cpuctl==0x10U;
  a.signature_slots_supported=s->booter_fuse==1U && s->fwsec_fuse==3U;
  a.differs_from_msi=s->ss0!=0x46254040U || (s->ss1!=2U && s->ss1!=3U);
  a.known_observation=s->chip==0xb72000a1U && a.gfw_idle &&
    s->wpr2_hi==0U && s->feature_plm==0xffffff8fU &&
    (s->ss0==0x10240014U || s->ss0==0x10241014U || s->ss0==0x10341014U ||
     s->ss0==0x10140014U) &&
    s->ss1==7U && s->plm_1183a4==0x2800U && a.signature_slots_supported;
  return a;
}
