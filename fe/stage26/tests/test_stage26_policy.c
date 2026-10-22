#include <Uefi.h>
#include "../engine/fe_cold.h"
#include <stdio.h>
static int test(UINT32 ss0, int context_ok, int legacy_ok) {
  GU_FE_COLD_SAMPLE s={0};
  s.chip=0xb72000a1U;
  s.gfw_plm=0x8b8fU; s.gfw_boot=0x3ffU; s.gfw_cpuctl=0x10U;
  s.feature_plm=0xffffff8fU; s.ss0=ss0; s.ss1=7U;
  s.plm_1183a4=0x2800U; s.booter_fuse=1U; s.fwsec_fuse=3U;
  GU_FE_COLD_ASSESSMENT a=GuFeAssessCold(&s);
  if (!!a.environment_ok_without_ss0 != context_ok || !!a.known_observation != legacy_ok) {
    fprintf(stderr,"ss0=%#x expected=%d/%d actual=%d/%d\n",ss0,context_ok,legacy_ok,
      !!a.environment_ok_without_ss0,!!a.known_observation);return 1;
  }
  return 0;
}
int main(void){
  const UINT32 varied[]={0x10140010U,0x10340010U,0x50340010U,0x10240010U,0x10040010U,0x10350010U};
  for(unsigned i=0;i<sizeof(varied)/sizeof(varied[0]);i++) if(test(varied[i],1,0)) return 1;
  if(test(0x10140014U,1,1) || test(0x10240014U,1,1) || test(0x10341014U,1,1))return 2;
  if(test(0xffffffffU,0,0))return 3;
  /* Every NON-SS0 safety guard remains mandatory. */
  GU_FE_COLD_SAMPLE s={0xb72000a1U,0x8b8fU,0x3ffU,0x10U,0U,0xffffff8fU,0x50340010U,7U,0x2800U,1U,3U};
#define REJECT(expr) do { expr; if(GuFeAssessCold(&s).environment_ok_without_ss0)return __LINE__; }while(0)
  REJECT(s.chip=0); s.chip=0xb72000a1U;
  REJECT(s.gfw_plm=0); s.gfw_plm=0x8b8fU;
  REJECT(s.gfw_boot=0); s.gfw_boot=0x3ffU;
  REJECT(s.gfw_cpuctl=0); s.gfw_cpuctl=0x10U;
  REJECT(s.wpr2_hi=1); s.wpr2_hi=0;
  REJECT(s.feature_plm=0); s.feature_plm=0xffffff8fU;
  REJECT(s.ss1=0); s.ss1=7;
  REJECT(s.plm_1183a4=0); s.plm_1183a4=0x2800U;
  REJECT(s.booter_fuse=0); s.booter_fuse=1;
  REJECT(s.fwsec_fuse=0);
  puts("PASS: SS0 raw logging profile only; six new SS0 values accepted; 10 independent guards still mandatory.");
  return 0;
}
