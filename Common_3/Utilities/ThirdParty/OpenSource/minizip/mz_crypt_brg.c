/* mz_crypt_brg.c -- Crypto/hash functions using Brian Gladman's library
   part of the MiniZip project

   Copyright (C) 2010-2020 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_os.h"

#if defined(_WINDOWS) || defined(XBOX)
#include <wincrypt.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#elif defined(NX64)
extern size_t nxGetCryptoRandom(void* buf, size_t bufLen, unsigned int flags);
static inline size_t getrandom(void* buf, size_t buflen, unsigned int flags)
{
	return nxGetCryptoRandom(buf, buflen, flags);
}
#elif defined(ORBIS) || defined(PROSPERO) || !defined(HAVE_GETRANDOM)
static inline size_t getrandom(void* buf, size_t buflen, unsigned int flags)
{
	FILE* file = fopen("/dev/urandom", "rb");
	if (!file)
		return 0;
	size_t size = fread(buf, 1, buflen, file);
	fclose(file);
	return size;
}
#else
#include <sys/random.h>
#endif

#include "lib/brg/aes.h"
#include "lib/brg/sha1.h"
#include "lib/brg/sha2.h"
#include "lib/brg/hmac.h"

#include "../../../Interfaces/ILog.h"

/***************************************************************************/
#if defined(_WINDOWS) || defined(XBOX)

int32_t mz_win_rand(uint8_t* buf, int32_t size)
{
	unsigned __int64 pentium_tsc[1];
	int32_t          len = 0;

	for (len = 0; len < (int)size; len += 1)
	{
		if (len % 8 == 0)
			QueryPerformanceCounter((LARGE_INTEGER*)pentium_tsc);
		buf[len] = ((unsigned char*)pentium_tsc)[len % 8];
	}

	return len;
}

int32_t mz_crypt_rand(uint8_t* buf, int32_t size)
{
	HCRYPTPROV provider;
	int32_t    result = 0;

	result = CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
	if (result)
	{
		result = CryptGenRandom(provider, size, buf);
		CryptReleaseContext(provider, 0);
		if (result)
			return size;
	}

	return mz_win_rand(buf, size);
}

#elif defined(__APPLE__)
int32_t mz_crypt_rand(uint8_t* buf, int32_t size)
{
	arc4random_buf(buf, (size_t)size);
	return size;
}

#else
int32_t mz_crypt_rand(uint8_t* buf, int32_t size)
{
	int32_t left = size;
	int32_t written = 0;

	while (left > 0)
	{
		written = getrandom(buf, left, 0);
		if (written == 0)
		{
			LOGF(eERROR, "Couldn't generate %d random numbers.", size);
			return 0;
		}

		buf += written;
		left -= written;
	}
	return size - left;
}
#endif

//#if defined(HAVE_ARC4RANDOM_BUF)
//int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
//    if (size < 0)
//        return 0;
//    arc4random_buf(buf, (uint32_t)size);
//    return size;
//}
//#elif defined(HAVE_ARC4RANDOM)
//int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
//    int32_t left = size;
//    for (; left > 2; left -= 3, buf += 3) {
//        uint32_t val = arc4random();
//
//        buf[0] = (val) & 0xFF;
//        buf[1] = (val >> 8) & 0xFF;
//        buf[2] = (val >> 16) & 0xFF;
//    }
//    for (; left > 0; left--, buf++) {
//        *buf = arc4random() & 0xFF;
//    }
//    return size - left;
//}
//#elif defined(HAVE_GETRANDOM)
//int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
//    int32_t left = size;
//    int32_t written = 0;
//
//    while (left > 0) {
//        written = getrandom(buf, left, 0);
//        if (written < 0)
//            return MZ_INTERNAL_ERROR;
//
//        buf += written;
//        left -= written;
//    }
//    return size - left;
//}
//#else
//#if !defined(FORCE_LOWQUALITY_ENTROPY)
//#pragma message("Warning: Low quality entropy function used for encryption")
//#endif
//int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
//    return mz_os_rand(buf, size);
//}
//#endif

/***************************************************************************/

typedef struct mz_crypt_sha_s
{
	sha256_ctx ctx256;
	sha1_ctx   ctx1;
	int32_t    initialized;
	uint16_t   algorithm;
} mz_crypt_sha;

/***************************************************************************/

void mz_crypt_sha_reset(void* handle)
{
	mz_crypt_sha* sha = (mz_crypt_sha*)handle;
	sha->initialized = 0;
}

bool mz_crypt_sha_begin(void* handle)
{
	mz_crypt_sha* sha = (mz_crypt_sha*)handle;

	if (sha == NULL)
		return false;

	if (sha->algorithm == MZ_HASH_SHA1)
		sha1_begin(&sha->ctx1);
	else
		sha256_begin(&sha->ctx256);

	sha->initialized = 1;
	return true;
}

size_t mz_crypt_sha_update(void* handle, const void* buf, size_t size)
{
	mz_crypt_sha* sha = (mz_crypt_sha*)handle;

	ASSERT(sha != NULL && buf != NULL && sha->initialized);

	if (sha->algorithm == MZ_HASH_SHA1)
		sha1_hash((const unsigned char*)buf, (unsigned long)size, &sha->ctx1);
	else
		sha256_hash((const unsigned char*)buf, (unsigned long)size, &sha->ctx256);

	return size;
}

bool mz_crypt_sha_end(void* handle, uint8_t* digest, size_t digest_size)
{
	mz_crypt_sha* sha = (mz_crypt_sha*)handle;

	if (sha == NULL || digest == NULL || !sha->initialized)
		return false;

	if (sha->algorithm == MZ_HASH_SHA1)
	{
		if (digest_size < MZ_HASH_SHA1_SIZE)
			return false;
		sha1_end(digest, &sha->ctx1);
	}
	else
	{
		if (digest_size < MZ_HASH_SHA256_SIZE)
			return false;
		sha256_end(digest, &sha->ctx256);
	}

	return true;
}

void mz_crypt_sha_set_algorithm(void* handle, uint16_t algorithm)
{
	mz_crypt_sha* sha = (mz_crypt_sha*)handle;
	sha->algorithm = algorithm;
}

void* mz_crypt_sha_create(void** handle)
{
	mz_crypt_sha* sha = NULL;

	sha = (mz_crypt_sha*)MZ_ALLOC(sizeof(mz_crypt_sha));
	if (sha != NULL)
	{
		memset(sha, 0, sizeof(mz_crypt_sha));
		sha->algorithm = MZ_HASH_SHA256;
	}
	if (handle != NULL)
		*handle = sha;

	return sha;
}

void mz_crypt_sha_delete(void** handle)
{
	mz_crypt_sha* sha = NULL;
	if (handle == NULL)
		return;
	sha = (mz_crypt_sha*)*handle;
	if (sha != NULL)
	{
		mz_crypt_sha_reset(*handle);
		MZ_FREE(sha);
	}
	*handle = NULL;
}

/***************************************************************************/

typedef struct mz_crypt_aes_s
{
	aes_encrypt_ctx encrypt_ctx;
	aes_decrypt_ctx decrypt_ctx;
	int32_t         mode;
	int32_t         error;
} mz_crypt_aes;

/***************************************************************************/

void mz_crypt_aes_reset(void* handle) { MZ_UNUSED(handle); }

size_t mz_crypt_aes_encrypt(void* handle, uint8_t* buf, size_t size)
{
	mz_crypt_aes* aes = (mz_crypt_aes*)handle;

	ASSERT(aes && buf);
	ASSERT(size == MZ_AES_BLOCK_SIZE);

	aes->error = aes_encrypt(buf, buf, &aes->encrypt_ctx);
	ASSERT(aes->error == 0);
	if (aes->error)
		return 0;
	return size;
}

size_t mz_crypt_aes_decrypt(void* handle, uint8_t* buf, size_t size)
{
	mz_crypt_aes* aes = (mz_crypt_aes*)handle;
	ASSERT(aes && buf);
	ASSERT(size == MZ_AES_BLOCK_SIZE);

	aes->error = aes_decrypt(buf, buf, &aes->decrypt_ctx);
	ASSERT(aes->error == 0);
	if (aes->error)
		return 0;
	return size;
}

bool mz_crypt_aes_set_encrypt_key(void* handle, const void* key, size_t key_length)
{
	mz_crypt_aes* aes = (mz_crypt_aes*)handle;

	if (aes == NULL || key == NULL)
		return false;

	mz_crypt_aes_reset(handle);

	aes->error = aes_encrypt_key((const unsigned char*)key, (int)key_length, &aes->encrypt_ctx);
	if (aes->error)
		return false;

	return true;
}

bool mz_crypt_aes_set_decrypt_key(void* handle, const void* key, size_t key_length)
{
	mz_crypt_aes* aes = (mz_crypt_aes*)handle;

	if (aes == NULL || key == NULL)
		return false;

	mz_crypt_aes_reset(handle);

	aes->error = aes_decrypt_key((const unsigned char*)key, (int)key_length, &aes->decrypt_ctx);
	if (aes->error)
		return false;

	return true;
}

void mz_crypt_aes_set_mode(void* handle, int32_t mode)
{
	mz_crypt_aes* aes = (mz_crypt_aes*)handle;
	aes->mode = mode;
}

void* mz_crypt_aes_create(void** handle)
{
	mz_crypt_aes* aes = NULL;

	aes = (mz_crypt_aes*)MZ_ALLOC(sizeof(mz_crypt_aes));
	if (aes != NULL)
		memset(aes, 0, sizeof(mz_crypt_aes));
	if (handle != NULL)
		*handle = aes;

	return aes;
}

void mz_crypt_aes_delete(void** handle)
{
	mz_crypt_aes* aes = NULL;
	if (handle == NULL)
		return;
	aes = (mz_crypt_aes*)*handle;
	if (aes != NULL)
		MZ_FREE(aes);
	*handle = NULL;
}

/***************************************************************************/

typedef struct mz_crypt_hmac_s
{
	hmac_ctx ctx;
	int32_t  initialized;
	int32_t  error;
	uint16_t algorithm;
} mz_crypt_hmac;

/***************************************************************************/

void mz_crypt_hmac_reset(void* handle)
{
	mz_crypt_hmac* hmac = (mz_crypt_hmac*)handle;
	hmac->error = 0;
}

bool mz_crypt_hmac_init(void* handle, const void* key, size_t key_length)
{
	mz_crypt_hmac* hmac = (mz_crypt_hmac*)handle;

	if (hmac == NULL)
		return false;

	mz_crypt_hmac_reset(handle);

	if (hmac->algorithm == MZ_HASH_SHA1)
		hmac_sha_begin(HMAC_SHA1, &hmac->ctx);
	else
		hmac_sha_begin(HMAC_SHA256, &hmac->ctx);

	hmac_sha_key((const unsigned char*)key, (unsigned long)key_length, &hmac->ctx);

	return true;
}

bool mz_crypt_hmac_update(void* handle, const void* buf, size_t size)
{
	mz_crypt_hmac* hmac = (mz_crypt_hmac*)handle;

	if (hmac == NULL || buf == NULL)
		return false;

	hmac_sha_data((const unsigned char*)buf, (unsigned long)size, &hmac->ctx);
	return true;
}

bool mz_crypt_hmac_end(void* handle, uint8_t* digest, size_t digest_size)
{
	mz_crypt_hmac* hmac = (mz_crypt_hmac*)handle;

	if (hmac == NULL || digest == NULL)
		return false;

	if (hmac->algorithm == MZ_HASH_SHA1)
	{
		if (digest_size < MZ_HASH_SHA1_SIZE)
			return false;
		hmac_sha_end(digest, (unsigned long)digest_size, &hmac->ctx);
	}
	else
	{
		if (digest_size < MZ_HASH_SHA256_SIZE)
			return false;
		hmac_sha_end(digest, (unsigned long)digest_size, &hmac->ctx);
	}

	return true;
}

void mz_crypt_hmac_set_algorithm(void* handle, uint16_t algorithm)
{
	mz_crypt_hmac* hmac = (mz_crypt_hmac*)handle;
	hmac->algorithm = algorithm;
}

bool mz_crypt_hmac_copy(void* src_handle, void* target_handle)
{
	mz_crypt_hmac* source = (mz_crypt_hmac*)src_handle;
	mz_crypt_hmac* target = (mz_crypt_hmac*)target_handle;

	if (target == NULL || source == NULL)
		return false;

	memcpy(&target->ctx, &source->ctx, sizeof(hmac_ctx));
	return true;
}

void* mz_crypt_hmac_create(void** handle)
{
	mz_crypt_hmac* hmac = NULL;

	hmac = (mz_crypt_hmac*)MZ_ALLOC(sizeof(mz_crypt_hmac));
	if (hmac != NULL)
	{
		memset(hmac, 0, sizeof(mz_crypt_hmac));
		hmac->algorithm = MZ_HASH_SHA256;
	}
	if (handle != NULL)
		*handle = hmac;

	return hmac;
}

void mz_crypt_hmac_delete(void** handle)
{
	mz_crypt_hmac* hmac = NULL;
	if (handle == NULL)
		return;
	hmac = (mz_crypt_hmac*)*handle;
	if (hmac != NULL)
		MZ_FREE(hmac);
	*handle = NULL;
}

/***************************************************************************/

#if defined(MZ_ZIP_SIGNING)
int32_t mz_crypt_sign(
	uint8_t* message, int32_t message_size, uint8_t* cert_data, int32_t cert_data_size, const char* cert_pwd, uint8_t** signature,
	int32_t* signature_size)
{
	MZ_UNUSED(message);
	MZ_UNUSED(message_size);
	MZ_UNUSED(cert_data);
	MZ_UNUSED(cert_data_size);
	MZ_UNUSED(cert_pwd);
	MZ_UNUSED(signature);
	MZ_UNUSED(signature_size);

	return MZ_SUPPORT_ERROR;
}

int32_t mz_crypt_sign_verify(uint8_t* message, int32_t message_size, uint8_t* signature, int32_t signature_size)
{
	MZ_UNUSED(message);
	MZ_UNUSED(message_size);
	MZ_UNUSED(signature);
	MZ_UNUSED(signature_size);

	return MZ_SUPPORT_ERROR;
}
#endif
