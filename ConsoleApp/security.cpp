#include "security.h"
#include "uECC/uECC.h"

#include <iostream>
#include <cstdlib>
#include <time.h>

// class functions duplicated from pc-ble-driver-js\src\driver_uecc.cpp

int rng(uint8_t *dest, unsigned size)
{
	for (unsigned i = 0; i < size; ++i)
	{
		dest[i] = rand() % 256;
	}

	return 1;
}

static uint8_t m_be_keys[ECC_P256_SK_LEN * 3];

void reverse(uint8_t* p_dst, uint8_t* p_src, uint32_t len)
{
	uint32_t i, j;

	for (i = len - 1, j = 0; j < len; i--, j++)
	{
		p_dst[j] = p_src[i];
	}
}

static bool isEccInitialized = false;

void ecc_init() {
	if (!isEccInitialized)
	{
		srand((unsigned int)time(NULL));
		uECC_set_rng(rng);
		isEccInitialized = true;
	}
}

int ecc_p256_gen_keypair(uint8_t* sk, uint8_t* pk) {
	const struct uECC_Curve_t * p_curve;

	uint8_t p_le_sk[ECC_P256_SK_LEN];   // Out
	uint8_t p_le_pk[ECC_P256_PK_LEN];   // Out

	p_curve = uECC_secp256r1();
	int ret = uECC_make_key((uint8_t *)&m_be_keys[ECC_P256_SK_LEN], (uint8_t *)&m_be_keys[0], p_curve);
	if (ret == 0) {
		return 0;
	}

	/* convert to little endian bytes and store in p_le_sk */
	reverse(&p_le_sk[0], &m_be_keys[0], ECC_P256_SK_LEN);
	/* convert to little endian bytes in 2 passes, store in p_le_pk */
	reverse(&p_le_pk[0], &m_be_keys[ECC_P256_SK_LEN], ECC_P256_SK_LEN);
	reverse(&p_le_pk[ECC_P256_SK_LEN], &m_be_keys[ECC_P256_SK_LEN * 2], ECC_P256_SK_LEN);

	memcpy_s(sk, ECC_P256_SK_LEN, p_le_sk, ECC_P256_SK_LEN);
	memcpy_s(pk, ECC_P256_PK_LEN, p_le_pk, ECC_P256_PK_LEN);
	return 1;
}

int ecc_p256_compute_pubkey(uint8_t* sk, uint8_t* pk) {
	if (sk == nullptr)
		return 0;

	const struct uECC_Curve_t * p_curve;
	//uint8_t *p_le_sk;   // In
	uint8_t p_le_sk[ECC_P256_SK_LEN];	// Duplicated data from In
	uint8_t p_le_pk[ECC_P256_PK_LEN];   // Out
	auto argumentcount = 0;

	memcpy_s(p_le_sk, ECC_P256_SK_LEN, sk, ECC_P256_SK_LEN);
	argumentcount++;

	p_curve = uECC_secp256r1();

	reverse(&m_be_keys[0], (uint8_t *)p_le_sk, ECC_P256_SK_LEN);
	//free(p_le_sk);

	//int ret = uECC_compute_public_key(p_le_sk, (uint8_t *)p_le_pk, p_curve);
	int ret = uECC_compute_public_key((uint8_t *)&m_be_keys[0], (uint8_t *)&m_be_keys[ECC_P256_SK_LEN], p_curve);
	if (ret == 0) {
		return 0;
	}

	/* convert to little endian bytes in 2 passes, store in m_be_keys */
	reverse(&p_le_pk[0], &m_be_keys[ECC_P256_SK_LEN], ECC_P256_SK_LEN);
	reverse(&p_le_pk[ECC_P256_SK_LEN], &m_be_keys[ECC_P256_SK_LEN * 2], ECC_P256_SK_LEN);

	memcpy_s(pk, ECC_P256_PK_LEN, p_le_pk, ECC_P256_PK_LEN);
	return 1;
}

int ecc_p256_compute_sharedsecret(uint8_t* ss) {
	//TODO:
	return 0;
}
