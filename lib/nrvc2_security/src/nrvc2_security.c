// The COMMS key stolen via physical means is no more harmful than the physical device being stolen. 
// Thus the COMMS key does not need to be secured in flash.
// If the TRC is stolen, the compromised key will not cause more harm because the vehicle is already stolen. 
// If the FOB is lost or stolen, the TRC can be deactivated and new key material can be created. 
// A lost or stolen FOB's key material compromise causes no more harm than the physical car key being stolen.

#include <nrvc2_security.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <NRVC2_KEY_COMMS.h>

#if defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == 1) // FOB
#include <NRVC2_ID_FOB.h>
#define DEV_UID NRVC2_ID_FOB
#elif defined(CONFIG_DEVICE_ROLE) && (CONFIG_DEVICE_ROLE == 2) // TRC
#include <NRVC2_ID_TRC.h>
#define DEV_UID NRVC2_ID_TRC
#else
#error CONFIG_DEVICE_ROLE is required for nrvc2_security compilation
#endif

LOG_MODULE_REGISTER(nrvc2_security);

// API TBD