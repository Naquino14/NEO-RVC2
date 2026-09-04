#ifndef __NRVC2_SECRETS_H__
#define __NRVC2_SECRETS_H__

#include <stdint.h>
#include <stdbool.h>

bool nrvc2_security_rdy();

int nrvc2_security_init();

// runs from device A first, called by some thread somewhere
int nrvc2_encrypt_and_sign(const uint8_t* pt, size_t pt_size, uint8_t* ct, size_t* ct_size, uint8_t* sig);

// when device B hears the frame, it ACKS the frame with a challenge 
// this computes the challenge itself, and it will be called by a thread somewhere in charge of the lora xceiver
int nrvc2_challenge(/* ??? */);

// when device A gets the challenge, it understands it as an ACK from device B
// the xceiver thread will call this to do the challenge
// if the challenge was ok, device B will reply with a MAC'd ACK with no challenge (sign only)
int nrvc2_do_challenge(/* ??? */);

// signs a message but doesn't encrypt it, saves processing power for things like sending ACKs and errors
int nrvc2_sign(const uint8_t* pt, size_t pt_size, uint8_t* sig);

// verifies if a sequence is legitimate
int nrvc2_verify(/* ??? */);

int nrvc2_decrypt_and_verify();

// API TBD

#endif