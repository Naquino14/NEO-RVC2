#! /bin/env python3

from time import sleep
from sys import argv
from secrets import token_bytes
from os import path, makedirs, chmod

SECRETS_DIR = "../lib/nrvc2_security/keys/"

ENTROPY_SOURCE = "/dev/urandom"

HEADER_TXT = """
#ifndef __NRVC2__SECRETS__%s__H__
#define __NRVC2__SECRETS__%s__H__

#include <stdint.h>

const uint8_t %s[%d] = {
%s
};

#endif // __NRVC2__SECRETS__%s__H__
"""

def gen_key_material(keyname, keylen):
    rng = token_bytes(keylen)
    rng_hex = ", ".join([f"0x{b:02x}" for b in rng])
    with open(SECRETS_DIR + keyname + ".h", "w") as f:
        f.write(HEADER_TXT % (keyname, keyname, keyname, len(rng), rng_hex, keyname))
    chmod(SECRETS_DIR + keyname + ".h", 0o600)

def main(argc, argv):
    if argc != 2:
        print("usage: gensecrets.py [comms]")
    else:
        if not path.exists(SECRETS_DIR):
            makedirs(SECRETS_DIR)

        print("Generating. Please wiggle your mouse or spam the keyboard to create entropy...")
        sleep(5)
        if argv[1] == "comms":
            # comms key is a 128 base shared secret used for derivation of session keys for comms between the TRC and FOB
            gen_key_material("NRVC2_KEY_COMMS_TRC2FOB", 16)
            gen_key_material("NRVC2_KEY_COMMS_FOB2TRC", 16)
        
        if argv[1] == "id_trc":
            # id_trc is a 64 bit UID for the TRC device
            gen_key_material("NRVC2_ID_TRC", 8)
        
        if argv[1] == "id_fob":
            # id_trc is a 64 bit UID for the FOB device
            gen_key_material("NRVC2_ID_FOB", 8)

if __name__ == "__main__":
    main(len(argv), argv)
    exit(0)