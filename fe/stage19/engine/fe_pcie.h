#pragma once
/* FE-only bounded PCIe capability parser. It never writes PCI config. */
#include "profiles.h"

typedef struct {
  UINT16 pcie_offset;
  BOOLEAN flr_supported;
  BOOLEAN flr_pending;
  BOOLEAN valid_chain;
} GU_FE_PCIE_OBSERVATION;

static inline UINT8 GuFeConfigByte(const UINT32 C[64], UINTN o) {
  return (UINT8)((C[o / 4U] >> (8U * (o & 3U))) & 0xffU);
}

static inline GU_FE_PCIE_OBSERVATION GuFeObservePcie(const UINT32 C[64]) {
  GU_FE_PCIE_OBSERVATION r={0,FALSE,FALSE,FALSE};
  UINT8 seen[48]={0};
  UINTN p,steps=0;
  if(!C || !(C[1] & 0x00100000U))return r; /* PCI status capabilities bit */
  p=GuFeConfigByte(C,0x34U);
  while (p) {
    if(p<0x40U || p>0xfcU || (p&3U) || ++steps>48)return r;
    UINTN index=(p-0x40U)/4U;
    if(seen[index])return r;
    seen[index]=1;
    UINT8 id=GuFeConfigByte(C,p),next=GuFeConfigByte(C,p+1U);
    if(id==0x10U) {
      /* PCIe capability >= 16 bytes; no config access outside 256 bytes. */
      if(p>0xf0U || r.pcie_offset) return r;
      r.pcie_offset=(UINT16)p;
      r.flr_supported=(C[(p+4U)/4U] & 0x10000000U)!=0;
      r.flr_pending=(C[(p+8U)/4U] & 0x00008000U)!=0;
    }
    p=next;
  }
  r.valid_chain=r.pcie_offset!=0;
  return r;
}
