/* mz_strm_wzaes.h -- Stream for WinZIP AES encryption
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_STREAM_WZAES_SHA1_H
#define MZ_STREAM_WZAES_SHA1_H

#ifdef __cplusplus
extern "C"
{
#endif

	/***************************************************************************/
	// Note: password should live as long as wzaes stream
	bool    mz_stream_wzaes_open(const char* password, int16_t encryptionMode, FileMode mode, FileStream* pBase, FileStream* pOut);
	size_t  mz_stream_wzaes_read(FileStream* stream, void* buf, size_t size);
	size_t  mz_stream_wzaes_write(FileStream* stream, const void* buf, size_t size);
	ssize_t mz_stream_wzaes_tell(const FileStream* stream);
	bool    mz_stream_wzaes_seek(FileStream* stream, SeekBaseOffset baseOffset, ssize_t seekOffset);
	bool    mz_stream_wzaes_close(FileStream* stream);
	bool    mz_stream_wzaes_flush(FileStream* stream);

	// Note: password should live as long as wzaes stream
	void mz_stream_wzaes_set_password(FileStream* stream, const char* password);
	void mz_stream_wzaes_set_encryption_mode(FileStream* stream, int16_t encryption_mode);

	bool mz_stream_wzaes_get_prop_int64(FileStream* stream, int32_t prop, int64_t* value);
	bool mz_stream_wzaes_set_prop_int64(FileStream* stream, int32_t prop, int64_t value);

	//void*   mz_stream_wzaes_create(void **stream);
	//void    mz_stream_wzaes_delete(void **stream);

	//void*   mz_stream_wzaes_get_interface(void);

	/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
