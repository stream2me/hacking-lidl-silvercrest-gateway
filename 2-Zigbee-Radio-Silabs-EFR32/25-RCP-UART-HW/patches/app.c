/**
 * @file app.c
 * @brief RCP 802.15.4 application - OpenThread RCP initialization
 *
 * Provides the required OpenThread instance creation and NCP initialization
 * for 802.15.4 RCP mode with CPC transport.
 *
 * Features:
 *   - Hardware Watchdog (2s timeout) for system reliability
 *   - GPIO status signaling for CPC activity monitoring
 */

#include "sl_component_catalog.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include <openthread-core-config.h>
#include <openthread/config.h>
#include <openthread/ncp.h>
#include <openthread/tasklet.h>
#include <openthread/instance.h>

#include "openthread-system.h"

#include "em_wdog.h"
#include "em_gpio.h"
#include "em_cmu.h"

/*******************************************************************************
 * Configuration - Status LED GPIO (optional)
 * Set to available GPIO pin if LED is connected, or leave undefined
 ******************************************************************************/
#define STATUS_LED_PORT     gpioPortF
#define STATUS_LED_PIN      4

/*******************************************************************************
 * Hardware Watchdog Configuration
 * Timeout: ~2 seconds using 1kHz ULFRCO
 ******************************************************************************/
static void wdog_init(void)
{
  WDOG_Init_TypeDef wdogInit = WDOG_INIT_DEFAULT;

  // Configure for ~2 second timeout
  // ULFRCO = 1kHz, perSel = wdogPeriod_2k = 2048 cycles = ~2.048s
  wdogInit.enable = true;
  wdogInit.debugRun = false;       // Stop in debug mode
  wdogInit.em2Run = true;          // Run in EM2
  wdogInit.em3Run = true;          // Run in EM3
  wdogInit.em4Block = false;
  wdogInit.swoscBlock = false;
  wdogInit.lock = false;
  wdogInit.clkSel = wdogClkSelULFRCO;  // 1kHz clock
  wdogInit.perSel = wdogPeriod_2k;     // ~2 seconds timeout

  // Enable WDOG clock and initialize
  CMU_ClockEnable(cmuClock_HFLE, true);
  WDOGn_Init(WDOG0, &wdogInit);
}

/*******************************************************************************
 * Status LED GPIO (optional hardware feature)
 ******************************************************************************/
static uint32_t sActivityCounter = 0;

static void status_led_init(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(STATUS_LED_PORT, STATUS_LED_PIN, gpioModePushPull, 0);
}

static void status_led_toggle(void)
{
  GPIO_PinOutToggle(STATUS_LED_PORT, STATUS_LED_PIN);
}

static void status_led_error_pattern(void)
{
  // Rapid blink pattern for errors (3 fast blinks)
  for (int i = 0; i < 6; i++) {
    GPIO_PinOutToggle(STATUS_LED_PORT, STATUS_LED_PIN);
    for (volatile int j = 0; j < 50000; j++);  // Short delay
  }
}

/*******************************************************************************
 * OpenThread Instance
 ******************************************************************************/

static otInstance *sInstance = NULL;

extern void otAppNcpInit(otInstance *aInstance);

otInstance *otGetInstance(void)
{
  return sInstance;
}

/**
 * @brief Create OpenThread instance (called by sl_ot_init)
 */
void sl_ot_create_instance(void)
{
  sInstance = otInstanceInitSingle();
  assert(sInstance);
}

/**
 * @brief Initialize NCP interface (called by sl_ot_init)
 */
void sl_ot_ncp_init(void)
{
  otAppNcpInit(sInstance);
}

/*******************************************************************************
 * Application Callbacks
 ******************************************************************************/

/**
 * @brief Application initialization
 */
void app_init(void)
{
  // Initialize hardware watchdog (2s timeout)
  wdog_init();

  // Initialize status LED GPIO
  status_led_init();

  // OpenThread RCP initialized by sl_ot_init() via sl_system_init()
}

/**
 * @brief Application process action (called from main loop)
 */
void app_process_action(void)
{
  // Feed the watchdog - system is alive
  WDOGn_Feed(WDOG0);

  // Process OpenThread tasks
  otTaskletsProcess(sInstance);
  otSysProcessDrivers(sInstance);

  // Toggle LED every ~256 iterations to show activity
  sActivityCounter++;
  if ((sActivityCounter & 0xFF) == 0) {
    status_led_toggle();
  }
}

/*******************************************************************************
 * CPC Error Callback (optional - called on CPC errors)
 ******************************************************************************/
void sl_cpc_on_error(void)
{
  // Signal CPC communication error via LED
  status_led_error_pattern();
}

/*******************************************************************************
 * STUBS for disabled CPC Security
 ******************************************************************************/

typedef enum {
  SL_CPC_SECURITY_STATE_NOT_READY = 0,
  SL_CPC_SECURITY_STATE_DISABLED = 1,
} sl_cpc_security_state_t;

sl_cpc_security_state_t sl_cpc_security_get_state(void)
{
  return SL_CPC_SECURITY_STATE_DISABLED;
}
