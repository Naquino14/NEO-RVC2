// The COMMS key stolen via physical means is no more harmful than the physical device being stolen. 
// Thus the COMMS key does not need to be secured in flash.
// If the TRC is stolen, the compromised key will not cause more harm because the vehicle is already stolen. 
// If the FOB is lost or stolen, the TRC can be deactivated and new key material can be created. 
// A lost or stolen FOB's key material compromise causes no more harm than the physical car key being stolen.

#include <nrvc2_security.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <mbedtls/ccm.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>

#include <NRVC2_KEY_COMMS_FOB2TRC.h>
static uint8_t session_key_comms_fob2trc[sizeof(NRVC2_KEY_COMMS_FOB2TRC)];
#include <NRVC2_KEY_COMMS_TRC2FOB.h>
static uint8_t session_key_comms_trc2fob[sizeof(NRVC2_KEY_COMMS_TRC2FOB)];

#include <nrvc2_errno.h>

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

static bool rdy = false;
bool nrvc2_security_rdy() {
    return rdy;
}

static int regen_session_key(uint8_t* session_key, const uint8_t* base_key, size_t key_len) {
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL)
        return -EKEYREGEN;

    int ret = mbedtls_hkdf(
        md_info, // message digest info
        NULL, 0, // no salt needed, gensecrets will generate uniformly random keys
        base_key, key_len, // IKM 
        NULL, 0, // Info stuff, unused
        session_key, key_len // OKM
    );

    return ret;
}

int nrvc2_security_init() {
    if (rdy)
        return -EALREADY;

    // expand base key(s)
    int ret = regen_session_key(
        session_key_comms_fob2trc, 
        NRVC2_KEY_COMMS_FOB2TRC, 
        sizeof(NRVC2_KEY_COMMS_FOB2TRC)
    );

    if (ret != 0) {
        LOG_ERR("key regen KEY_COMMS_FOB2TRC failed: %d", ret);
        return -EKEYREGEN;
    }

    // printk("FOB2TRC: 0x%02x", session_key_comms_fob2trc[0]);
    // for (int i = 1; i < sizeof(session_key_comms_fob2trc); i++)
    //     printk("%02x", session_key_comms_fob2trc[i]);
    // printk("\n");

    ret = regen_session_key(
        session_key_comms_trc2fob, 
        NRVC2_KEY_COMMS_TRC2FOB,
        sizeof(NRVC2_KEY_COMMS_TRC2FOB)
    );

    // printk("TRC2FOB: 0x%02x", session_key_comms_trc2fob[0]);
    // for (int i = 1; i < sizeof(session_key_comms_trc2fob); i++)
    //     printk("%02x", session_key_comms_trc2fob[i]);
    // printk("\n");

    if (ret != 0) {
        LOG_ERR("key regen KEY_COMMS_TRC2FOB failed: %d", ret);
        return -EKEYREGEN;
    }

    // ...

    rdy = true;
    return 0;
}

int nrvc2_security_deinit() {
    if (!rdy)
        return -EALREADY;
    
    // stuff ?
    
    rdy = false;
    return 0;
}