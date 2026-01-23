/***************************************************************************//**
 * @file app.c
 * @brief Zigbee 3.0 Router application callbacks
 *******************************************************************************
 * Minimal router application that:
 * - Automatically attempts to join an existing Zigbee network
 * - Routes messages between devices in the mesh
 * - Supports end devices as children
 ******************************************************************************/

#include "app/framework/include/af.h"
#include "app/framework/plugin/network-steering/network-steering.h"

// Event for delayed network steering start
static sl_zigbee_event_t networkSteeringEventControl;
static void networkSteeringEventHandler(sl_zigbee_event_t *event);

/***************************************************************************//**
 * Application initialization callback
 * Called once during startup
 ******************************************************************************/
void emberAfMainInitCallback(void)
{
  // Initialize the network steering event
  sl_zigbee_event_init(&networkSteeringEventControl, networkSteeringEventHandler);

  // Schedule network steering to start after 3 seconds
  // This gives the stack time to fully initialize
  sl_zigbee_event_set_delay_ms(&networkSteeringEventControl, 3000);
}

/***************************************************************************//**
 * Stack status change callback
 * Called when the network status changes
 ******************************************************************************/
void emberAfStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP) {
    // Successfully joined a network - cancel any pending steering
    sl_zigbee_event_set_inactive(&networkSteeringEventControl);
  } else if (status == EMBER_NETWORK_DOWN) {
    // Lost network connection - schedule rejoin attempt
    sl_zigbee_event_set_delay_ms(&networkSteeringEventControl, 1000);
  }
}

/***************************************************************************//**
 * Network steering complete callback
 * Called when network steering finishes (success or failure)
 ******************************************************************************/
void emberAfPluginNetworkSteeringCompleteCallback(EmberStatus status,
                                                   uint8_t totalBeacons,
                                                   uint8_t joinAttempts,
                                                   uint8_t finalState)
{
  (void)totalBeacons;
  (void)joinAttempts;
  (void)finalState;

  if (status != EMBER_SUCCESS) {
    // Failed to join - retry after 10 seconds
    sl_zigbee_event_set_delay_ms(&networkSteeringEventControl, 10000);
  }
}

/***************************************************************************//**
 * Radio calibration callback
 * Called when the radio needs calibration
 ******************************************************************************/
void emberAfRadioNeedsCalibratingCallback(void)
{
  // Use the MAC layer calibration function
  sl_mac_calibrate_current_channel();
}

/***************************************************************************//**
 * Network steering event handler
 * Starts network steering if not already on a network
 ******************************************************************************/
static void networkSteeringEventHandler(sl_zigbee_event_t *event)
{
  sl_zigbee_event_set_inactive(event);

  // Check if we're already on a network
  EmberNetworkStatus state = emberAfNetworkState();
  if (state == EMBER_JOINED_NETWORK) {
    return;
  }

  // Start network steering to find and join a network
  EmberStatus status = emberAfPluginNetworkSteeringStart();

  if (status != EMBER_SUCCESS) {
    // Failed to start steering - retry after 5 seconds
    sl_zigbee_event_set_delay_ms(event, 5000);
  }
}
