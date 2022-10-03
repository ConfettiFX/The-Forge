/* mz_strm_wzaes.c -- Stream for WinZip AES encryption
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng
   Copyright (C) 1998-2010 Brian Gladman, Worcester, UK

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_crypt.h"
#include "mz_strm.h"
#include "mz_strm_wzaes.h"

#include "../../../Interfaces/ILog.h"
#include "../../../Interfaces/IMemory.h"

/***************************************************************************/

#define MZ_AES_KEYING_ITERATIONS (1000)
#define MZ_AES_SALT_LENGTH(MODE) (4 * (MODE & 3) + 4)
#define MZ_AES_SALT_LENGTH_MAX (16)
#define MZ_AES_PW_LENGTH_MAX (128)
#define MZ_AES_PW_VERIFY_SIZE (2)
#define MZ_AES_AUTHCODE_SIZE (10)

/***************************************************************************/
static IFileSystem mz_stream_wzaes_vtbl = { NULL,    // Open
											mz_stream_wzaes_close,
											mz_stream_wzaes_read,
											mz_stream_wzaes_write,
											mz_stream_wzaes_seek,
											mz_stream_wzaes_tell,
											NULL,    // GetFileSize
											mz_stream_wzaes_flush,
											NULL,    // IsAtEnd
											NULL,    // GetResourceMount
											mz_stream_wzaes_get_prop_int64,
											mz_stream_wzaes_set_prop_int64 };

/***************************************************************************/

typedef struct mz_stream_wzaes_s
{
	int32_t     mode;
	int32_t     error;
	int16_t     initialized;
	uint8_t     buffer[UINT16_MAX];
	int64_t     total_in;
	int64_t     max_total_in;
	int64_t     total_out;
	int16_t     encryption_mode;
	const char* password;
	void*       aes;
	uint32_t    crypt_pos;
	uint8_t     crypt_block[MZ_AES_BLOCK_SIZE];
	void*       hmac;
	uint8_t     nonce[MZ_AES_BLOCK_SIZE];
} mz_stream_wzaes;

/***************************************************************************/

static bool mz_stream_wzaes_cleanup(FileStream* pStream, bool ret_val)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)pStream->pUser;
	if (wzaes != NULL)
	{
		mz_crypt_aes_delete(&wzaes->aes);
		mz_crypt_hmac_delete(&wzaes->hmac);
		MZ_FREE(wzaes);
	}

	memset(pStream, 0, sizeof(*pStream));
	return ret_val;
}

bool mz_stream_wzaes_open(const char* password, int16_t encryptionMode, FileMode mode, FileStream* pBase, FileStream* stream)
{
	ASSERT(pBase && stream && password);

	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)MZ_ALLOC(sizeof(mz_stream_wzaes));
	if (wzaes == NULL)
		return false;

	stream->pUser = wzaes;
	stream->pBase = pBase;

	memset(wzaes, 0, sizeof(mz_stream_wzaes));
	stream->pIO = &mz_stream_wzaes_vtbl;
	wzaes->encryption_mode = MZ_AES_ENCRYPTION_MODE_256;

	mz_crypt_hmac_create(&wzaes->hmac);
	mz_crypt_aes_create(&wzaes->aes);

	uint16_t salt_length = 0;
	uint16_t password_length = 0;
	uint16_t key_length = 0;
	uint8_t  kbuf[2 * MZ_AES_KEY_LENGTH_MAX + MZ_AES_PW_VERIFY_SIZE];
	uint8_t  verify[MZ_AES_PW_VERIFY_SIZE];
	uint8_t  verify_expected[MZ_AES_PW_VERIFY_SIZE];
	uint8_t  salt_value[MZ_AES_SALT_LENGTH_MAX];
	mz_stream_wzaes_set_password(stream, password);
	mz_stream_wzaes_set_encryption_mode(stream, encryptionMode);

	wzaes->total_in = 0;
	wzaes->total_out = 0;
	wzaes->initialized = 0;

	password_length = (uint16_t)strlen(password);
	if (password_length > MZ_AES_PW_LENGTH_MAX)
		return mz_stream_wzaes_cleanup(stream, false);

	if (wzaes->encryption_mode < 1 || wzaes->encryption_mode > 3)
		return mz_stream_wzaes_cleanup(stream, false);

	salt_length = MZ_AES_SALT_LENGTH(wzaes->encryption_mode);

	if (mode & FM_WRITE)
	{
		if (mz_crypt_rand(salt_value, salt_length) != salt_length)
			return mz_stream_wzaes_cleanup(stream, false);
	}
	else if (mode & FM_READ)
	{
		if (fsReadFromStream(stream->pBase, salt_value, salt_length) != salt_length)
			return mz_stream_wzaes_cleanup(stream, false);
	}

	key_length = MZ_AES_KEY_LENGTH(wzaes->encryption_mode);

	/* Derive the encryption and authentication keys and the password verifier */
	mz_crypt_pbkdf2(
		(uint8_t*)password, password_length, salt_value, salt_length, MZ_AES_KEYING_ITERATIONS, kbuf,
		2 * key_length + MZ_AES_PW_VERIFY_SIZE);

	/* Initialize the encryption nonce and buffer pos */
	wzaes->crypt_pos = MZ_AES_BLOCK_SIZE;
	memset(wzaes->nonce, 0, sizeof(wzaes->nonce));

	/* Initialize for encryption using key 1 */
	mz_crypt_aes_reset(wzaes->aes);
	mz_crypt_aes_set_mode(wzaes->aes, wzaes->encryption_mode);
	mz_crypt_aes_set_encrypt_key(wzaes->aes, kbuf, key_length);

	/* Initialize for authentication using key 2 */
	mz_crypt_hmac_reset(wzaes->hmac);
	mz_crypt_hmac_set_algorithm(wzaes->hmac, MZ_HASH_SHA1);
	mz_crypt_hmac_init(wzaes->hmac, kbuf + key_length, key_length);

	memcpy(verify, kbuf + (2 * key_length), MZ_AES_PW_VERIFY_SIZE);

	if (mode & FM_WRITE)
	{
		if (fsWriteToStream(stream->pBase, salt_value, salt_length) != salt_length)
			return mz_stream_wzaes_cleanup(stream, false);

		wzaes->total_out += salt_length;

		if (fsWriteToStream(stream->pBase, verify, MZ_AES_PW_VERIFY_SIZE) != MZ_AES_PW_VERIFY_SIZE)
			return mz_stream_wzaes_cleanup(stream, false);

		wzaes->total_out += MZ_AES_PW_VERIFY_SIZE;
	}
	else if (mode & FM_READ)
	{
		wzaes->total_in += salt_length;

		if (fsReadFromStream(stream->pBase, verify_expected, MZ_AES_PW_VERIFY_SIZE) != MZ_AES_PW_VERIFY_SIZE)
			return mz_stream_wzaes_cleanup(stream, false);

		wzaes->total_in += MZ_AES_PW_VERIFY_SIZE;

		if (memcmp(verify_expected, verify, MZ_AES_PW_VERIFY_SIZE) != 0)
			return mz_stream_wzaes_cleanup(stream, false);
	}

	wzaes->mode = mode;
	wzaes->initialized = 1;

	return true;
}

static bool mz_stream_wzaes_ctr_encrypt(FileStream* stream, uint8_t* buf, size_t size)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	uint32_t         pos = wzaes->crypt_pos;
	size_t           i = 0;

	while (i < size)
	{
		if (pos == MZ_AES_BLOCK_SIZE)
		{
			uint32_t j = 0;

			/* Increment encryption nonce */
			while (j < 8 && !++wzaes->nonce[j])
				j += 1;

			/* Encrypt the nonce to form next xor buffer */
			memcpy(wzaes->crypt_block, wzaes->nonce, MZ_AES_BLOCK_SIZE);
			mz_crypt_aes_encrypt(wzaes->aes, wzaes->crypt_block, sizeof(wzaes->crypt_block));
			pos = 0;
		}

		buf[i++] ^= wzaes->crypt_block[pos++];
	}

	wzaes->crypt_pos = pos;
	return true;
}

size_t mz_stream_wzaes_read(FileStream* stream, void* buf, size_t size)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	int64_t          max_total_in = 0;
	size_t           bytes_to_read = size;
	size_t           read = 0;

	max_total_in = wzaes->max_total_in - MZ_AES_FOOTER_SIZE;
	if ((int64_t)bytes_to_read > (max_total_in - wzaes->total_in))
		bytes_to_read = (size_t)(max_total_in - wzaes->total_in);

	read = fsReadFromStream(stream->pBase, buf, bytes_to_read);

	if (read > 0)
	{
		mz_crypt_hmac_update(wzaes->hmac, (uint8_t*)buf, read);
		mz_stream_wzaes_ctr_encrypt(stream, (uint8_t*)buf, read);

		wzaes->total_in += read;
	}

	return read;
}

size_t mz_stream_wzaes_write(FileStream* stream, const void* buf, size_t size)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	const uint8_t*   buf_ptr = (const uint8_t*)buf;
	size_t           bytes_to_write = sizeof(wzaes->buffer);
	size_t           total_written = 0;
	size_t           written = 0;

	if (size < 0)
		return false;

	do
	{
		if (bytes_to_write > (size - total_written))
			bytes_to_write = (size - total_written);

		memcpy(wzaes->buffer, buf_ptr, bytes_to_write);
		buf_ptr += bytes_to_write;

		mz_stream_wzaes_ctr_encrypt(stream, (uint8_t*)wzaes->buffer, bytes_to_write);
		mz_crypt_hmac_update(wzaes->hmac, wzaes->buffer, bytes_to_write);

		written = fsWriteToStream(stream->pBase, wzaes->buffer, bytes_to_write);
		if (written < 0)
			return written;

		total_written += written;
	} while (total_written < size && written > 0);

	wzaes->total_out += total_written;
	return total_written;
}

ssize_t mz_stream_wzaes_tell(const FileStream* stream) { return fsGetStreamSeekPosition(stream->pBase); }

bool mz_stream_wzaes_seek(FileStream* stream, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	return fsSeekStream(stream->pBase, baseOffset, seekOffset);
}

bool mz_stream_wzaes_flush(FileStream* stream)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	if (wzaes->initialized)
	{
		uint8_t expected_hash[MZ_AES_AUTHCODE_SIZE];
		uint8_t computed_hash[MZ_HASH_SHA1_SIZE];

		mz_crypt_hmac_end(wzaes->hmac, computed_hash, sizeof(computed_hash));

		if (wzaes->mode & FM_WRITE)
		{
			if (fsWriteToStream(stream->pBase, computed_hash, MZ_AES_AUTHCODE_SIZE) != MZ_AES_AUTHCODE_SIZE)
				return mz_stream_wzaes_cleanup(stream, false);

			wzaes->total_out += MZ_AES_AUTHCODE_SIZE;
		}
		else if (wzaes->mode & FM_READ)
		{
			if (fsReadFromStream(stream->pBase, expected_hash, MZ_AES_AUTHCODE_SIZE) != MZ_AES_AUTHCODE_SIZE)
				return mz_stream_wzaes_cleanup(stream, false);

			wzaes->total_in += MZ_AES_AUTHCODE_SIZE;

			/* If entire entry was not read this will fail */
			if (memcmp(computed_hash, expected_hash, MZ_AES_AUTHCODE_SIZE) != 0)
				return mz_stream_wzaes_cleanup(stream, false);
		}

		wzaes->initialized = 0;
	}
	return true;
}

bool mz_stream_wzaes_close(FileStream* stream)
{
	mz_stream_wzaes_flush(stream);
	return mz_stream_wzaes_cleanup(stream, true);
}

//int32_t mz_stream_wzaes_error(void *stream) {
//    mz_stream_wzaes *wzaes = (mz_stream_wzaes *)stream;
//    return wzaes->error;
//}

void mz_stream_wzaes_set_password(FileStream* stream, const char* password)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	wzaes->password = password;
}

void mz_stream_wzaes_set_encryption_mode(FileStream* stream, int16_t encryption_mode)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	wzaes->encryption_mode = encryption_mode;
}

bool mz_stream_wzaes_get_prop_int64(FileStream* stream, int32_t prop, int64_t* value)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	switch (prop)
	{
		case MZ_STREAM_PROP_TOTAL_IN: *value = wzaes->total_in; break;
		case MZ_STREAM_PROP_TOTAL_OUT: *value = wzaes->total_out; break;
		case MZ_STREAM_PROP_TOTAL_IN_MAX: *value = wzaes->max_total_in; break;
		case MZ_STREAM_PROP_HEADER_SIZE: *value = MZ_AES_SALT_LENGTH((int64_t)wzaes->encryption_mode) + MZ_AES_PW_VERIFY_SIZE; break;
		case MZ_STREAM_PROP_FOOTER_SIZE: *value = MZ_AES_AUTHCODE_SIZE; break;
		default: return false;
	}
	return true;
}

bool mz_stream_wzaes_set_prop_int64(FileStream* stream, int32_t prop, int64_t value)
{
	mz_stream_wzaes* wzaes = (mz_stream_wzaes*)stream->pUser;
	switch (prop)
	{
		case MZ_STREAM_PROP_TOTAL_IN_MAX: wzaes->max_total_in = value; break;
		default: return false;
	}
	return true;
}

//void *mz_stream_wzaes_create(void **stream) {
//    mz_stream_wzaes *wzaes = NULL;
//
//    wzaes = (mz_stream_wzaes *)MZ_ALLOC(sizeof(mz_stream_wzaes));
//    if (wzaes != NULL) {
//        memset(wzaes, 0, sizeof(mz_stream_wzaes));
//        wzaes->stream.vtbl = &mz_stream_wzaes_vtbl;
//        wzaes->encryption_mode = MZ_AES_ENCRYPTION_MODE_256;
//
//        mz_crypt_hmac_create(&wzaes->hmac);
//        mz_crypt_aes_create(&wzaes->aes);
//    }
//    if (stream != NULL)
//        *stream = wzaes;
//
//    return wzaes;
//}

void mz_stream_wzaes_delete(void** stream)
{
	mz_stream_wzaes* wzaes = NULL;
	if (stream == NULL)
		return;
	wzaes = (mz_stream_wzaes*)*stream;
	if (wzaes != NULL)
	{
		mz_crypt_aes_delete(&wzaes->aes);
		mz_crypt_hmac_delete(&wzaes->hmac);
		MZ_FREE(wzaes);
	}
	*stream = NULL;
}

void* mz_stream_wzaes_get_interface(void) { return (void*)&mz_stream_wzaes_vtbl; }
