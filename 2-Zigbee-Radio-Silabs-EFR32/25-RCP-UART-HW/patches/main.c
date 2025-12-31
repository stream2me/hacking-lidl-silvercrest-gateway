/**
 * @file main.c
 * @brief RCP 802.15.4 main entry point for EFR32MG1B
 *
 * RTL8196E Compatibility:
 * - 1 second boot delay allows the RTL8196E to initialize its UART
 */

#include "sl_system_init.h"
#include "sl_system_process_action.h"
#include "sl_udelay.h"

extern void app_init(void);
extern void app_process_action(void);

int main(void)
{
  // RTL8196E boot delay: wait 1 second for host UART to be ready
  sl_udelay_wait(1000000);

  sl_system_init();
  app_init();

  while (1) {
    sl_system_process_action();
    app_process_action();
  }
}
