#ifndef __NRVC2_SECRETS_H__
#define __NRVC2_SECRETS_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool nrvc2_security_rdy();

int nrvc2_security_init();

/// @todo MOVE THIS TO A KCONFIG OPTION
#define CONFIG_NRVC2_SECURITY_SEQUENCE_WINDOW 10llu

#define NRVC2_SECURITY_TAG_SIZE 16

typedef enum {
    /// @brief Session communications key from the FOB to the TRC
    NRVC2_KEYOPT_SESS_COMMS_FOB2TRC,
    /// @brief Session communications key from the TRC to the FOB
    NRVC2_KEYOPT_SESS_COMMS_TRC2FOB
} keyopt_t;

/**
 * Encrypts and signs plaintext `pt` of size `pt_size` as ciphertext `ct_out`. Computed MAC signature gets stored in `sig_out`.
 * @param key the key material to use in the cryptographic operation
 * @param pt the plaintext to encrypt and compute a mac 
 * @param pt_len the size of the plaintext `pt`
 * @param ct_out the buffer to store the ciphertext in, shgould be at least `pt_len` in size
 * @param tag_out the buffer to store the ciphertext signature, must be at least `NRVC2_SECURITY_MAC_SIZE` bytes in size
 * @returns 0 on success, -EINVAL when parameters are invalid
 */
int nrvc2_security_encrypt_and_sign(const keyopt_t key, const uint8_t* pt, const size_t pt_size, uint8_t* ct_out, uint8_t tag_out[NRVC2_SECURITY_TAG_SIZE]);

/**
 * Verifies if a message is legitimate and decrypts it.
 * @param key the key material to use in the cryptographic operation
 * @param ct the ciphertext to verify and decrypt
 * @param ct_size the size of the ciphertext buffer
 * @param seqn the incoming sequence number the message is tied to
 * @param tag the incoming MAC of the ciphertext to verify
 * @param pt_out pointer to the buffer to stire the plaintext, should be of size `ct_size`
 */
int nrvc2_security_decrypt_and_verify(const keyopt_t key, const uint8_t* ct, const size_t ct_size, const uint64_t seqn, const uint8_t tag[NRVC2_SECURITY_TAG_SIZE], uint8_t* pt_out);

#endif