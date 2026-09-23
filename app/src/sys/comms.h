#ifndef NRVC2_COMMS_H
#define NRVC2_COMMS_H

#include <stdint.h>
#include <stdbool.h>

// wraps lora and nrvc2_security functions

/**
 * Check if comms system is ready
 * @returns true when ready, false otherwise
 */
bool comms_rdy();

/**
 * Put comms system in BIT mode
 * @param bit_running true if BIT is running, false to re-init comms system
 */
void comms_bit_mode(bool bit_running);

// sets up transmit work queue (with timeouts, priorities?), any structs, register receive interrupts
int comms_init();

// transmit a buffer. it will encrypt the buffer, tag it, and package it with the seqn in
// this format
// magic & tag & seqn & ciphertext
// this function just take a semaphore and queues work for the internal function transmit_work
// transmit_work will put the transceiver in receive mode after transmitting
// and hold on to the semaphore until a valid ACK is received or the transmission times out
int comms_transmit(uint8_t* txbuf, size_t txbuf_len);

// takes the transceover semaphore, transmits an ack, puts the receiver in receive mode
// then increments the semaphore
// static void transmit_ack();

// receives a buffer, check the magic to see if its a command or an ack, 
// auths it, decrypts it, and releases the semaphore
// static void receive_work();

#endif // !NRVC2_COMMS_H