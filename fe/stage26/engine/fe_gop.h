#pragma once
/*
 * FE-only device-path correlation. Read-only observation, never authorization.
 * Compare an AMD display PCI controller's UEFI device path to the GOP path:
 *   controller = ACPI(root) / PCI(...) [/ PCI(...)] / End
 *   GOP        = same-prefix / ACPI(ADR) [/ ACPI(ADR)] / End
 *
 * Does not guess a fixed PCI bus number, root slot, GOP handle or framebuffer.
 * A matching path does not prove physical monitor routing or unlock safety.
 */
#include <Uefi.h>
#include <Protocol/DevicePath.h>

static inline UINT16 GuFePathNodeLength(const UINT8 *p, UINTN remaining) {
  UINT16 len;
  if (!p || remaining < 4) return 0;
  len = (UINT16)(p[2] | ((UINT16)p[3] << 8));
  return len >= 4 && len <= remaining ? len : 0;
}

static inline BOOLEAN GuFePathEqualBytes(const UINT8 *a, const UINT8 *b, UINTN n) {
  for (UINTN i=0; i<n; ++i) if (a[i] != b[i]) return FALSE;
  return TRUE;
}

static inline BOOLEAN GuFeGopMatchesController(const EFI_DEVICE_PATH_PROTOCOL *Controller,
    const EFI_DEVICE_PATH_PROTOCOL *Gop, UINTN MaxBytes) {
  const UINT8 *c=(const UINT8 *)Controller, *g=(const UINT8 *)Gop;
  UINTN cr=MaxBytes,gr=MaxBytes,pci_hops=0,adr_nodes=0;
  if (!c || !g || MaxBytes < 22 || MaxBytes > 4096) return FALSE;
  /* The observed system uses an ACPI PNP0A03/PNP0A08 root node. */
  UINT16 root=GuFePathNodeLength(c,cr);
  if (root!=12 || GuFePathNodeLength(g,gr)!=12 ||
      c[0]!=0x02 || c[1]!=0x01 ||
      !GuFePathEqualBytes(c,g,root) ||
      (!((c[4]==0xd0 && c[5]==0x41 && c[6]==0x03 && c[7]==0x0a) ||
         (c[4]==0xd0 && c[5]==0x41 && c[6]==0x08 && c[7]==0x0a))))return FALSE;
  if (c[8] || c[9] || c[10] || c[11])return FALSE;
  c+=root;g+=root;cr-=root;gr-=root;
  for (;;) {
    UINT16 cl=GuFePathNodeLength(c,cr),gl=GuFePathNodeLength(g,gr);
    if (!cl || !gl) return FALSE;
    if (c[0]==0x7f) {
      if (c[1]!=0xff || cl!=4 || !pci_hops) return FALSE;
      break;
    }
    if (cl!=6 || gl!=6 || c[0]!=0x01 || c[1]!=0x01 ||
        !GuFePathEqualBytes(c,g,cl) || ++pci_hops>4)return FALSE;
    c+=cl;g+=gl;cr-=cl;gr-=gl;
  }
  for (;;) {
    UINT16 gl=GuFePathNodeLength(g,gr);
    if (!gl) return FALSE;
    if (g[0]==0x7f) return g[1]==0xff && gl==4 && adr_nodes>0;
    if (g[0]!=0x02 || g[1]!=0x03 || gl<8 || (gl&3) || ++adr_nodes>4)
      return FALSE;
    g+=gl;gr-=gl;
  }
}
