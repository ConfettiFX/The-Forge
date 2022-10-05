/* mz_strm_zlib.c -- Stream for zlib inflate/deflate
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_zlib.h"

#include "../../../Interfaces/ILog.h"

#include "../zip/miniz.h"

/***************************************************************************/

#if defined(ZLIBNG_VERNUM) && !defined(ZLIB_COMPAT)
#define ZLIB_PREFIX(x) zng_##x
typedef zng_stream zlib_stream;
#else
#define ZLIB_PREFIX(x) x
typedef z_stream zlib_stream;
#endif

#if !defined(DEF_MEM_LEVEL)
#if MAX_MEM_LEVEL >= 8
#define DEF_MEM_LEVEL 8
#else
#define DEF_MEM_LEVEL MAX_MEM_LEVEL
#endif
#endif

//static bool mzStreamZlibOpen(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);
static bool    mzStreamZlibCloseOriginal(FileStream* pFile);
static bool    mzStreamZlibClose(FileStream* pFile);
static size_t  mzStreamZlibRead(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes);
static size_t  mzStreamZlibWrite(FileStream* pFile, const void* sourceBuffer, size_t byteCount);
static bool    mzStreamZlibSeek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset);
static ssize_t mzStreamZlibTell(const FileStream* pFile);
//static ssize_t mzStreamZlibGetFileSize(const FileStream* pFile);
static bool mzStreamZlibFlushOriginal(FileStream* pFile);
static bool mzStreamZlibFlush(FileStream* pFile);
//static bool mzStreamZlibIsAtEnd(const FileStream* pFile);

static bool mzStreamZlibGetPropInt64(FileStream* pFile, int32_t prop, int64_t* pValue);
static bool mzStreamZlibSetPropInt64(FileStream* pFile, int32_t prop, int64_t value);

/***************************************************************************/

static IFileSystem mzZlibIO = { NULL,    // Open
								mzStreamZlibClose,
								mzStreamZlibRead,
								mzStreamZlibWrite,
								mzStreamZlibSeek,
								mzStreamZlibTell,
								NULL,    // GetFileSize
								mzStreamZlibFlush,
								NULL,    // IsAtEnd
								NULL,    // GetResourceMount
								mzStreamZlibGetPropInt64,
								mzStreamZlibSetPropInt64 };

/***************************************************************************/

typedef struct mz_stream_zlib_s
{
	zlib_stream zstream;
	uint8_t     buffer[INT16_MAX];
	int32_t     buffer_len;
	int64_t     total_in;
	int64_t     total_out;
	int64_t     max_total_in;
	int8_t      initialized;
	int16_t     level;
	int32_t     window_bits;
	int32_t     mode;
	int32_t     error;
} mz_stream_zlib;

/***************************************************************************/

bool mzStreamZlibOpen(FileStream* pBase, FileMode mode, FileStream* pOut)
{
	if (!pOut)
		return false;
	mz_stream_zlib* zlib = NULL;

	zlib = (mz_stream_zlib*)MZ_ALLOC(sizeof(mz_stream_zlib));
	if (zlib != NULL)
	{
		memset(zlib, 0, sizeof(mz_stream_zlib));
		pOut->pIO = &mzZlibIO;
		//zlib->stream.vtbl = &mz_stream_zlib_vtbl;
		zlib->level = Z_DEFAULT_COMPRESSION;
		zlib->window_bits = -MAX_WBITS;
	}

	pOut->pUser = zlib;

	zlib->zstream.data_type = 0;
	zlib->zstream.zalloc = Z_NULL;
	zlib->zstream.zfree = Z_NULL;
	zlib->zstream.opaque = Z_NULL;
	zlib->zstream.total_in = 0;
	zlib->zstream.total_out = 0;

	zlib->total_in = 0;
	zlib->total_out = 0;

	ASSERT(!(mode & FM_WRITE) != !(mode & FM_READ));

	if (mode & FM_WRITE)
	{
#ifdef MZ_ZIP_NO_COMPRESSION
		MZ_FREE(zlib);
		memset(pOut, 0, sizeof(*pOut));
		return MZ_SUPPORT_ERROR;
#else
		zlib->zstream.next_out = zlib->buffer;
		zlib->zstream.avail_out = sizeof(zlib->buffer);

		zlib->error = ZLIB_PREFIX(deflateInit2)(
			&zlib->zstream, (int8_t)zlib->level, Z_DEFLATED, zlib->window_bits, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
#endif
	}
	else if (mode & FM_READ)
	{
#ifdef MZ_ZIP_NO_DECOMPRESSION
		return false;
#else
		zlib->zstream.next_in = zlib->buffer;
		zlib->zstream.avail_in = 0;

		zlib->error = ZLIB_PREFIX(inflateInit2)(&zlib->zstream, zlib->window_bits);
#endif
	}

	if (zlib->error != Z_OK)
	{
		MZ_FREE(zlib);
		memset(pOut, 0, sizeof(*pOut));
		return false;
	}

	zlib->initialized = 1;
	zlib->mode = mode;
	pOut->pBase = pBase;
	return true;
}

//int32_t mz_stream_zlib_is_open(void *stream) {
//    mz_stream_zlib *zlib = (mz_stream_zlib *)stream;
//    if (zlib->initialized != 1)
//        return MZ_OPEN_ERROR;
//    return MZ_OK;
//}

// int32_t mzStreamZlibRead(void *stream, void *buf, int32_t size) {
static size_t mzStreamZlibRead(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes)
{
#ifdef MZ_ZIP_NO_DECOMPRESSION
	MZ_UNUSED(stream);
	MZ_UNUSED(buf);
	MZ_UNUSED(size);
	return false;
#else
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;
	uint64_t        total_in_before = 0;
	uint64_t        total_in_after = 0;
	uint64_t        total_out_before = 0;
	uint64_t        total_out_after = 0;
	uint32_t        total_out = 0;
	uint32_t        in_bytes = 0;
	uint32_t        out_bytes = 0;
	int32_t         bytes_to_read = sizeof(zlib->buffer);
	size_t          read = 0;
	int32_t         err = Z_OK;

	zlib->zstream.next_out = (Bytef*)outputBuffer;
	zlib->zstream.avail_out = (uInt)bufferSizeInBytes;

	do
	{
		if (zlib->zstream.avail_in == 0)
		{
			if (zlib->max_total_in > 0)
			{
				if ((int64_t)bytes_to_read > (zlib->max_total_in - zlib->total_in))
					bytes_to_read = (int32_t)(zlib->max_total_in - zlib->total_in);
			}

			read = fsReadFromStream(pFile->pBase, zlib->buffer, bytes_to_read);

			if (read < 0)
				return read;

			zlib->zstream.next_in = zlib->buffer;
			zlib->zstream.avail_in = (unsigned int)read;
		}

		total_in_before = zlib->zstream.avail_in;
		total_out_before = zlib->zstream.total_out;

		err = ZLIB_PREFIX(inflate)(&zlib->zstream, Z_SYNC_FLUSH);
		if ((err >= Z_OK) && (zlib->zstream.msg != NULL))
		{
			zlib->error = Z_DATA_ERROR;
			break;
		}

		total_in_after = zlib->zstream.avail_in;
		total_out_after = zlib->zstream.total_out;

		in_bytes = (uint32_t)(total_in_before - total_in_after);
		out_bytes = (uint32_t)(total_out_after - total_out_before);

		total_out += out_bytes;

		zlib->total_in += in_bytes;
		zlib->total_out += out_bytes;

		if (err == Z_STREAM_END)
			break;
		if (err != Z_OK)
		{
			zlib->error = err;
			break;
		}
	} while (zlib->zstream.avail_out > 0);

	if (zlib->error != 0)
	{
		/* Zlib errors are compatible with MZ */
		return 0;
	}

	return total_out;
#endif
}

#ifndef MZ_ZIP_NO_COMPRESSION
static bool mzStreamZlibFlushOriginal(FileStream* pFile)
{
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;
	if (fsWriteToStream(pFile->pBase, zlib->buffer, zlib->buffer_len) != zlib->buffer_len)
		return false;
	return fsFlushStream(pFile->pBase);
}
bool mzStreamZlibFlush(FileStream* pFile)
{
	bool            noerr = true;
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;
	if (zlib->initialized)
	{
		noerr |= mzStreamZlibCloseOriginal(pFile);
	}
	return noerr;
}

static bool mzStreamZlibDeflate(FileStream* pStream, int flush)
{
	mz_stream_zlib* zlib = (mz_stream_zlib*)pStream->pUser;
	uint64_t        total_out_before = 0;
	uint64_t        total_out_after = 0;
	int32_t         out_bytes = 0;
	int32_t         zerr = Z_OK;

	do
	{
		if (zlib->zstream.avail_out == 0)
		{
			if (!mzStreamZlibFlushOriginal(pStream))
				return false;

			zlib->zstream.avail_out = sizeof(zlib->buffer);
			zlib->zstream.next_out = zlib->buffer;

			zlib->buffer_len = 0;
		}

		total_out_before = zlib->zstream.total_out;
		zerr = ZLIB_PREFIX(deflate)(&zlib->zstream, flush);
		total_out_after = zlib->zstream.total_out;

		out_bytes = (uint32_t)(total_out_after - total_out_before);

		zlib->buffer_len += out_bytes;
		zlib->total_out += out_bytes;

		if (zerr == Z_STREAM_END)
			break;
		if (zerr != Z_OK)
		{
			zlib->error = zerr;
			return false;
		}
	} while ((zlib->zstream.avail_in > 0) || (flush == Z_FINISH && zerr == Z_OK));

	return true;
}
#endif

//int32_t mzStreamZlibWrite(void *stream, const void *buf, int32_t size) {
static size_t mzStreamZlibWrite(FileStream* pFile, const void* sourceBuffer, size_t byteCount)
{
#ifdef MZ_ZIP_NO_COMPRESSION
	MZ_UNUSED(stream);
	MZ_UNUSED(buf);
	MZ_UNUSED(size);
	return MZ_SUPPORT_ERROR;
#else
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;

	zlib->zstream.next_in = (Bytef*)(intptr_t)sourceBuffer;
	zlib->zstream.avail_in = (uInt)byteCount;

	if (!mzStreamZlibDeflate(pFile, Z_NO_FLUSH))
		return 0;

	zlib->total_in += byteCount;
	return byteCount;
#endif
}

//int64_t mzStreamZlibTell(void *stream) {
static ssize_t mzStreamZlibTell(const FileStream* pFile)
{
	//MZ_UNUSED(stream);
	LOGF(eWARNING, "Trying to use mzStreamZlibTell function.");
	return -1;
}

//int32_t mzStreamZlibSeek(void *stream, int64_t offset, int32_t origin) {
static bool mzStreamZlibSeek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	MZ_UNUSED(pFile);
	MZ_UNUSED(baseOffset);
	MZ_UNUSED(seekOffset);

	LOGF(eWARNING, "Trying to use mzStreamZlibSeek function.");

	return false;
}

//int32_t mzStreamZlibClose(void *stream) {
static bool mzStreamZlibCloseOriginal(FileStream* pFile)
{
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;

	if (zlib->mode & FM_WRITE)
	{
#ifdef MZ_ZIP_NO_COMPRESSION
		return MZ_SUPPORT_ERROR;
#else
		mzStreamZlibDeflate(pFile, Z_FINISH);
		mzStreamZlibFlushOriginal(pFile);

		ZLIB_PREFIX(deflateEnd)(&zlib->zstream);
#endif
	}
	else if (zlib->mode & FM_READ)
	{
#ifdef MZ_ZIP_NO_DECOMPRESSION
		return MZ_SUPPORT_ERROR;
#else
		ZLIB_PREFIX(inflateEnd)(&zlib->zstream);
#endif
	}

	zlib->initialized = 0;

	return zlib->error == Z_OK;
}
static bool mzStreamZlibClose(FileStream* pFile)
{
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;
	bool            noerr = true;
	if (zlib->initialized)
		noerr |= mzStreamZlibCloseOriginal(pFile);

	MZ_FREE(pFile->pUser);
	memset(pFile, 0, sizeof(*pFile));

	return noerr;
}

//int32_t mz_stream_zlib_error(void *stream) {
//    mz_stream_zlib *zlib = (mz_stream_zlib *)stream;
//    return zlib->error;
//}

//int32_t mz_stream_zlib_get_prop_int64(void *stream, int32_t prop, int64_t *value) {
static bool mzStreamZlibGetPropInt64(FileStream* pFile, int32_t prop, int64_t* pValue)
{
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;
	switch (prop)
	{
		case MZ_STREAM_PROP_TOTAL_IN: *pValue = zlib->total_in; break;
		case MZ_STREAM_PROP_TOTAL_IN_MAX: *pValue = zlib->max_total_in; break;
		case MZ_STREAM_PROP_TOTAL_OUT: *pValue = zlib->total_out; break;
		case MZ_STREAM_PROP_HEADER_SIZE: *pValue = 0; break;
		case MZ_STREAM_PROP_COMPRESS_WINDOW: *pValue = zlib->window_bits; break;
		default: return false;
	}
	return true;
}

//int32_t mz_stream_zlib_set_prop_int64(void *stream, int32_t prop, int64_t value) {
static bool mzStreamZlibSetPropInt64(FileStream* pFile, int32_t prop, int64_t value)
{
	mz_stream_zlib* zlib = (mz_stream_zlib*)pFile->pUser;
	switch (prop)
	{
		case MZ_STREAM_PROP_COMPRESS_LEVEL: zlib->level = (int16_t)value; break;
		case MZ_STREAM_PROP_TOTAL_IN_MAX: zlib->max_total_in = value; break;
		case MZ_STREAM_PROP_COMPRESS_WINDOW: zlib->window_bits = (int32_t)value; break;
		default: return false;
	}
	return true;
}

//void *mz_stream_zlib_create(void **stream) {
//    mz_stream_zlib *zlib = NULL;
//
//    zlib = (mz_stream_zlib *)MZ_ALLOC(sizeof(mz_stream_zlib));
//    if (zlib != NULL) {
//        memset(zlib, 0, sizeof(mz_stream_zlib));
//        zlib->stream.vtbl = &mz_stream_zlib_vtbl;
//        zlib->level = Z_DEFAULT_COMPRESSION;
//        zlib->window_bits = -MAX_WBITS;
//    }
//    if (stream != NULL)
//        *stream = zlib;
//
//    return zlib;
//}
//
//void mz_stream_zlib_delete(void **stream) {
//    mz_stream_zlib *zlib = NULL;
//    if (stream == NULL)
//        return;
//    zlib = (mz_stream_zlib *)*stream;
//    if (zlib != NULL)
//        MZ_FREE(zlib);
//    *stream = NULL;
//}

//void *mz_stream_zlib_get_interface(void) {
//    return (void *)&mz_stream_zlib_vtbl;
//}
