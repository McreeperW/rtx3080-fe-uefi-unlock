#include <assert.h>
#include <stdio.h>
#include <string.h>
#define efi_main stage13b_unused_main
#include "../src/bootbridge.c"

int main(void){
  UINT8 boot[256]={0}, fs[256]={0};
  const UINT8 *hd=NULL,*hd2=NULL;
  UINTN hlen=0,hlen2=0,end=0,end2=0;
  /* Simulate Boot0000 HD(partition)/File(Windows)/End */
  boot[0]=4;boot[1]=1;boot[2]=42;boot[3]=0;
  boot[4]=1;boot[5]=0;boot[6]=0;boot[7]=0;
  for(UINTN j=8;j<42;j++)boot[j]=(UINT8)(j+23);
  const CHAR16 p[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
  UINTN flen=4+sizeof(p);
  assert(42+flen+4<sizeof(boot));
  boot[42]=4;boot[43]=4;boot[44]=(UINT8)flen;boot[45]=(UINT8)(flen>>8);
  memcpy(boot+46,p,sizeof(p));
  boot[42+flen]=0x7f;boot[43+flen]=0xff;boot[44+flen]=4;
  UINTN bn=42+flen+4;
  assert(path_is_windows(boot,bn));
  assert(path_hd_node(boot,bn,&hd,&hlen,&end));
  assert(hd==boot&&hlen==42&&end==bn-4);
  /* Simulate a controller-level path prefixed to the same HD node */
  fs[0]=1;fs[1]=1;fs[2]=6;fs[3]=0;
  memcpy(fs+6,boot,42);
  fs[48]=0x7f;fs[49]=0xff;fs[50]=4;
  assert(path_hd_node(fs,52,&hd2,&hlen2,&end2));
  assert(hlen2==42&&end2==48&&bytes_equal(hd,hd2,42));
  fs[6+20]^=1;assert(!bytes_equal(hd,hd2,42));
  fs[6+20]^=1;
  /* Garbage/truncated nodes are rejected */
  boot[2]=2;assert(!path_hd_node(boot,bn,&hd,&hlen,&end));boot[2]=42;
  assert(!path_hd_node(boot,bn-1,&hd,&hlen,&end));
  boot[0]=3;assert(path_hd_node(boot,bn,&hd,&hlen,&end)&&hd==NULL);
  printf("PASS Stage13b: Boot#### abbreviated HD, full filesystem HD match, mismatch/truncation rejection\n");
  return 0;
}
