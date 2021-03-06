#pragma once
#include <stdint.h>

#define ECC_P256_SK_LEN 32 /*BLE_GAP_LESC_P256_SK_LEN*/
#define ECC_P256_PK_LEN 64 /*BLE_GAP_LESC_P256_PK_LEN*/

void ecc_init();
// generate private key and public key
int ecc_p256_gen_keypair(uint8_t* sk, uint8_t* pk);
// given private key to get public key
int ecc_p256_compute_pubkey(uint8_t* sk, uint8_t* pk);
int ecc_p256_valid_public_key(uint8_t* pk);
// given key pair to get shared secret
int ecc_p256_compute_sharedsecret(uint8_t* sk, uint8_t* pk, uint8_t* ss);
