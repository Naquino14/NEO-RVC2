// The COMMS key stolen via physical means is no more harmful than the physical device being stolen. 
// Thus the COMMS key does not need to be secured in flash.
// If the TRC is stolen, the compromised key will not cause more harm because the vehicle is already stolen. 
// If the FOB is lost or stolen, the TRC can be deactivated and new key material can be created. 
// A lost or stolen FOB's key material compromise causes no more harm than the physical car key being stolen.

#include <nrvc2_security.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
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

/// @todo for the future, this needs to be persistent in flash
static uint64_t fob2trc_sequence_num;
static uint64_t trc2fob_sequence_num;

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

static uint8_t* keyopt_to_keymat(const keyopt_t keyopt) {
    switch (keyopt) {
        case NRVC2_KEYOPT_SESS_COMMS_FOB2TRC:
            return session_key_comms_fob2trc;
        case NRVC2_KEYOPT_SESS_COMMS_TRC2FOB:
            return session_key_comms_trc2fob;
        default:
            return NULL;
    }
}

static size_t keyopt_to_keylen(const keyopt_t keyopt) {
    switch (keyopt) {
        case NRVC2_KEYOPT_SESS_COMMS_FOB2TRC:
            return sizeof(session_key_comms_fob2trc);
        case NRVC2_KEYOPT_SESS_COMMS_TRC2FOB:
            return sizeof(session_key_comms_trc2fob);
        default:
            return 0;
    }
}

static uint64_t keyopt_to_comms_seqn(const keyopt_t keyopt) {
    switch (keyopt) {
        case NRVC2_KEYOPT_SESS_COMMS_FOB2TRC:
            return fob2trc_sequence_num;
        case NRVC2_KEYOPT_SESS_COMMS_TRC2FOB:
            return trc2fob_sequence_num;
        default:
            return 0xffffffffffffffff;
    }
}

static void increment_seqn(const keyopt_t keyopt) {
    switch (keyopt) {
        case NRVC2_KEYOPT_SESS_COMMS_FOB2TRC:
            ++fob2trc_sequence_num;
            break;
        case NRVC2_KEYOPT_SESS_COMMS_TRC2FOB:
            ++trc2fob_sequence_num;
            break;
        default:
            break;
    }
}

static void update_seqn(const keyopt_t keyopt, uint64_t new_seqn) {
    switch (keyopt) {
        case NRVC2_KEYOPT_SESS_COMMS_FOB2TRC:
            fob2trc_sequence_num = new_seqn;
            break;
        case NRVC2_KEYOPT_SESS_COMMS_TRC2FOB:
            trc2fob_sequence_num = new_seqn;
            break;
        default:
            break;
    }
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

    ret = regen_session_key(
        session_key_comms_trc2fob, 
        NRVC2_KEY_COMMS_TRC2FOB,
        sizeof(NRVC2_KEY_COMMS_TRC2FOB)
    );

    if (ret != 0) {
        LOG_ERR("key regen KEY_COMMS_TRC2FOB failed: %d", ret);
        return -EKEYREGEN;
    }

    /// @todo for the future, read this value from flash memory
    trc2fob_sequence_num = 0;
    fob2trc_sequence_num = 0;

    // ...

    rdy = true;
    return 0;
}

int nrvc2_security_encrypt_and_sign(const keyopt_t key, const uint8_t* pt, const size_t pt_size, uint8_t* ct_out, uint8_t tag_out[NRVC2_SECURITY_TAG_SIZE]) {
    uint64_t iv = keyopt_to_comms_seqn(key);
    uint8_t* keymat = keyopt_to_keymat(key);
    size_t keylen = keyopt_to_keylen(key);
    uint64_t seqnum = keyopt_to_comms_seqn(key);

    mbedtls_ccm_context ccm_context;
    mbedtls_ccm_init(&ccm_context);
    
    int ret = mbedtls_ccm_setkey(&ccm_context, MBEDTLS_CIPHER_ID_AES, keymat, keylen * 8);
    if (ret < 0) {
        LOG_ERR("Internal mbedtls error during encrypt setkey: %d", ret);
        mbedtls_ccm_free(&ccm_context);
        return -ECRYPTO;
    }

    ret = mbedtls_ccm_encrypt_and_tag(&ccm_context,
        pt_size, 
        (uint8_t*)&iv, sizeof(uint64_t),
        (uint8_t*)&seqnum, sizeof(uint64_t),
        pt,
        ct_out, 
        tag_out, NRVC2_SECURITY_TAG_SIZE);

    mbedtls_ccm_free(&ccm_context);
    
    if (ret < 0) {
        LOG_ERR("Internal mbedtls error during encrypt and tag: %d", ret);
        return -ECRYPTO;
    }

    increment_seqn(key);

    return 0;
}

int nrvc2_security_compute_challenge(const keyopt_t key, const uint32_t seqn, uint8_t* challenge_out) {
    return 0;
}

int nrvc2_security_do_challenge(const keyopt_t key, uint8_t* challenge, const uint32_t seqn, uint8_t* response_out, uint8_t tag_out[NRVC2_SECURITY_TAG_SIZE]) {
    return 0;
}

int nrvc2_security_decrypt_and_verify(const keyopt_t key, const uint8_t* ct, const size_t ct_size, const uint64_t seqn, const uint8_t tag[NRVC2_SECURITY_TAG_SIZE], uint8_t* pt_out) {
    uint8_t* keymat = keyopt_to_keymat(key);
    size_t keylen = keyopt_to_keylen(key);
    uint64_t seqnum_cur = keyopt_to_comms_seqn(key);

    // verify sequence number first
    if (seqn <= seqnum_cur || seqn > (seqnum_cur + CONFIG_NRVC2_SECURITY_SEQUENCE_WINDOW))
        return -ESEQUENCE;

    uint64_t iv = seqn; 
    
    mbedtls_ccm_context ccm_context;
    mbedtls_ccm_init(&ccm_context);

    int ret = mbedtls_ccm_setkey(&ccm_context, MBEDTLS_CIPHER_ID_AES, keymat, keylen * 8);
    if (ret < 0) {
        LOG_ERR("Internal mbedtls error during decrypt setkey: %d", ret);
        mbedtls_ccm_free(&ccm_context);
        return -ECRYPTO;
    }

    ret = mbedtls_ccm_auth_decrypt(&ccm_context,
        ct_size,
        (uint8_t*)&iv, sizeof(uint64_t),
        (uint8_t*)&seqn, sizeof(uint64_t),
        ct,
        pt_out,
        tag, NRVC2_SECURITY_TAG_SIZE);
    
    mbedtls_ccm_free(&ccm_context);

    if (ret == MBEDTLS_ERR_CCM_AUTH_FAILED)
        return -EAUTH;
    else if (ret < 0) {
        LOG_ERR("Internal mbedtls error during decrypt and auth: %d", ret);
        return -ECRYPTO;
    }

    update_seqn(key, seqn);
    return 0;
}

int nrvc2_security_deinit() {
    if (!rdy)
        return -EALREADY;
    
    // stuff ?
    
    rdy = false;
    return 0;
}