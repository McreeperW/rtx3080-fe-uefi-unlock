#include "../src/bootbridge.c"
#include <stdio.h>
#include <stdlib.h>
static unsigned char buf[256];
static size_t build(const char *name){
    size_t i=0;
    /* Example vendor-ish nonpath hardware node */
    buf[i++]=0x01;buf[i++]=0x01;buf[i++]=0x04;buf[i++]=0x00;
    size_t z=0;while(name[z])z++;
    size_t n=4+(z+1)*2;
    buf[i++]=0x04;buf[i++]=0x04;buf[i++]=(unsigned char)n;buf[i++]=(unsigned char)(n>>8);
    for(size_t j=0;j<=z;j++){buf[i++]=(unsigned char)name[j];buf[i++]=0;}
    buf[i++]=0x7f;buf[i++]=0xff;buf[i++]=0x04;buf[i++]=0;
    return i;
}
int main(void){
    size_t n=build("\\EFI\\Microsoft\\Boot\\bootmgfw.efi");
    if(!path_is_windows(buf,n))return 1;
    n=build("\\efi\\MICROSOFT\\boot\\BOOTMGFW.EFI");
    if(!path_is_windows(buf,n))return 2;
    n=build("\\EFI\\BOOT\\BOOTX64.EFI");
    if(path_is_windows(buf,n))return 3;
    n=build("\\EFI\\Microsoft\\Boot\\bootmgfw.efi");buf[n-3]=0x00;
    if(path_is_windows(buf,n))return 4;
    puts("PASS: Windows Boot Manager path selected; non-Windows and malformed paths rejected");
    return 0;
}
