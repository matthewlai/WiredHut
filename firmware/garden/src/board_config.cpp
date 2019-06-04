#include "ostrich.h"

#include <libopencm3/stm32/rcc.h>

namespace Ostrich {
BoardConfig MakeBoardConfig() {
  BoardConfig bc;

  bc.clock_scale = rcc_3v3[RCC_CLOCK_3V3_216MHZ];
  bc.hse_mhz = 12;
  bc.use_hse = true;

  // 1 ms.
  bc.systick_period_clocks = 216000;

  bc.vdd_voltage_mV = 3300;

  return bc;
}
}
