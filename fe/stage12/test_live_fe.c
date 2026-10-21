#include "checks.h"
#include "profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int load(const char *p, UINT32 c[64]) {
 FILE *f=fopen(p,"rb"); char s[256];int n=0;unsigned long long off,val;
 if(!f)return 0;
 while(fgets(s,sizeof(s),f)){
  if(sscanf(s,"config_offset=0x%llx",&off)!=1)continue;
  if(!fgets(s,sizeof(s),f)||sscanf(s,"value=0x%llx",&val)!=1||off!=(unsigned)n*4||n==64){fclose(f);return 0;}
  c[n++]=(UINT32)val;
 }
 fclose(f);return n==64;
}
int main(int argc,char **argv){
 UINT32 config[64]={0};GU_CAPS caps;
 if(argc!=2||!load(argv[1],config))return 3;
 printf("profile=%d decode=%d guard=%d\n",(int)GuIdentifyProfile(config,0,16,0,0),
 (int)GuProfileMemoryDecodeEnabled(config), (int)GuCheckConfig(config,0,16,0,0,&caps));
 if(!GuCheckConfig(config,0,16,0,0,&caps))return 2;
 printf("caps pm=%#x pcie=%#x msi=%#x msix=%#x pcie_ver=%u\n",caps.pm,caps.pcie,caps.msi,caps.msix,caps.pcie_version);
 config[1]|=2;
 if(!GuCheckConfig(config,0,16,0,0,&caps))return 2;
 printf("PASS observed disabled and enabled MSE accepted in EXPERIMENTAL profile\n");
 return 0;
}
