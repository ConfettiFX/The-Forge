/* mz_crypt.h -- Crypto/hash functions
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_CRYPT_H
#define MZ_CRYPT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/***************************************************************************/

	uint32_t mz_crypt_crc32_update(uint32_t value, const uint8_t* buf, size_t size);

	bool mz_crypt_pbkdf2(
		uint8_t* password, size_t password_length, uint8_t* salt, size_t salt_length, size_t iteration_count, uint8_t* key,
		size_t key_length);

	/***************************************************************************/

	int32_t mz_crypt_rand(uint8_t* buf, int32_t size);

	void   mz_crypt_sha_reset(void* handle);
	bool   mz_crypt_sha_begin(void* handle);
	size_t mz_crypt_sha_update(void* handle, const void* buf, size_t size);
	bool   mz_crypt_sha_end(void* handle, uint8_t* digest, size_t digest_size);
	void   mz_crypt_sha_set_algorithm(void* handle, uint16_t algorithm);
	void*  mz_crypt_sha_create(void** handle);
	void   mz_crypt_sha_delete(void** handle);

	void   mz_crypt_aes_reset(void* handle);
	size_t mz_crypt_aes_encrypt(void* handle, uint8_t* buf, size_t size);
	size_t mz_crypt_aes_decrypt(void* handle, uint8_t* buf, size_t size);
	bool   mz_crypt_aes_set_encrypt_key(void* handle, const void* key, size_t key_length);
	bool   mz_crypt_aes_set_decrypt_key(void* handle, const void* key, size_t key_length);
	void   mz_crypt_aes_set_mode(void* handle, int32_t mode);
	void*  mz_crypt_aes_create(void** handle);
	void   mz_crypt_aes_delete(void** handle);

	void  mz_crypt_hmac_reset(void* handle);
	bool  mz_crypt_hmac_init(void* handle, const void* key, size_t key_length);
	bool  mz_crypt_hmac_update(void* handle, const void* buf, size_t size);
	bool  mz_crypt_hmac_end(void* handle, uint8_t* digest, size_t digest_size);
	bool  mz_crypt_hmac_copy(void* src_handle, void* target_handle);
	void  mz_crypt_hmac_set_algorithm(void* handle, uint16_t algorithm);
	void* mz_crypt_hmac_create(void** handle);
	void  mz_crypt_hmac_delete(void** handle);

	bool mz_crypt_sign(
		uint8_t* message, int32_t message_size, uint8_t* cert_data, int32_t cert_data_size, const char* cert_pwd, uint8_t** signature,
		int32_t* signature_size);
	bool mz_crypt_sign_verify(uint8_t* message, int32_t message_size, uint8_t* signature, int32_t signature_size);

	/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
