#include <Uefi.h>
#include "../engine/fe_cold.h"
static int check(GU_FE_COLD_SAMPLE *s, int good) {
  return ((GuFeAssessCold(s).known_observation != 0) == good) ? 0 : 1;
}
int main(void) {
  GU_FE_COLD_SAMPLE s={0};
  s.chip=0xb72000a1U; s.gfw_plm=0x8b8fU; s.gfw_boot=0x3ffU;
  s.gfw_cpuctl=0x10U; s.wpr2_hi=0U; s.feature_plm=0xffffff8fU;
  s.ss0=0x10140014U; s.ss1=7U; s.plm_1183a4=0x2800U;
  s.booter_fuse=1U; s.fwsec_fuse=3U;
  if(check(&s,1)) return 1;
  s.ss0=0x10341014U; if(check(&s,1)) return 2;
  s.ss0=0x00000000U; if(check(&s,0)) return 3;
  s.ss0=0x10140014U; s.wpr2_hi=1U; if(check(&s,0)) return 4;
  s.wpr2_hi=0U; s.plm_1183a4=0U; if(check(&s,0)) return 5;
  s.plm_1183a4=0x2800U; s.gfw_boot=0U; if(check(&s,0)) return 6;
  s.gfw_boot=0x3ffU; s.fwsec_fuse=0U; if(check(&s,0)) return 7;
  return 0;
}
