#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include <nrvc2_security.h>
#include <nrvc2_errno.h>

#include "built-in-test.h"
#include "roles.h"

LOG_MODULE_REGISTER(main);

int main(void) {
    printk("Waking up...\n\n");
    
    k_msleep(2 * 1000);

    bool configured = role_config();
    if (!configured) {
        LOG_ERR("ROLE CFG FAIL");
    }
    
    bit_basic();

    // comms test
    const char pt[] = "The quick brown fox jumped over the lazy dog!\n";
    uint8_t ct[sizeof(pt)];
    uint8_t pt_out[sizeof(pt)];
    const keyopt_t keyopt = NRVC2_KEYOPT_SESS_COMMS_TRC2FOB;
    uint8_t tag[NRVC2_SECURITY_TAG_SIZE];

    // encryption test
    int ret = nrvc2_security_encrypt_and_sign(keyopt, pt, sizeof(pt), ct, tag);
    printk("encrypt and sign 1 returns %d\n", ret);

    printk("sign 1: 0x");
    for (uint8_t* tb = tag; tb < tag + sizeof(tag); tb++)
        printk("%02x", *tb);
    printk("\n");

    printk("ct 1: 0x");
    for (uint8_t* cb = ct; cb < ct + sizeof(ct); cb++)
        printk("%02x", *cb);
    printk("\n");

    // decryption test
    int seqn = 0;
    ret = nrvc2_security_decrypt_and_verify(keyopt, ct, sizeof(ct), seqn, tag, pt_out);
    if (ret == -EAUTH)
        printk("Auth error!\n");
    if (ret == -ESEQUENCE)
        printk("Sequence number error!\n");

    
    printk("pt: '");
    for (char* c = pt_out; c < pt_out + sizeof(pt_out); c++)
        printk("%c", *c);
    printk("'\n");


    return 0;
}