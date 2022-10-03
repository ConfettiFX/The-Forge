/* mz_strm.c -- Stream interface
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
	 https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_strm.h"

#include "../../../Interfaces/ILog.h"
#include "../../../Interfaces/IMemory.h"

typedef struct mz_stream_raw_s
{
	int64_t total_in;
	int64_t total_out;
	int64_t max_total_in;
} mz_stream_raw;

static IFileSystem mz_stream_raw_vtbl = {
	NULL,
	mz_stream_raw_close,
	mz_stream_raw_read,
	mz_stream_raw_write,
	mz_stream_raw_seek,
	mz_stream_raw_tell,
	NULL,
	mz_stream_raw_flush,
	NULL,
	NULL,
	mz_stream_raw_get_prop_int64,
	mz_stream_raw_set_prop_int64,
};

bool mz_stream_raw_open(FileStream* pBase, FileStream* pOut)
{
	ASSERT(pBase && pOut);
	mz_stream_raw* raw = NULL;

	raw = (mz_stream_raw*)MZ_ALLOC(sizeof(mz_stream_raw));
	if (raw == NULL)
		return false;

	memset(raw, 0, sizeof(mz_stream_raw));
	pOut->pUser = raw;
	pOut->pIO = &mz_stream_raw_vtbl;

	pOut->pBase = pBase;

	return true;
}

//bool mz_stream_raw_is_open(void *stream) {
//	mz_stream_raw *raw = (mz_stream_raw *)stream;
//	return mz_stream_is_open(raw->stream.base);
//}

size_t mz_stream_raw_read(FileStream* stream, void* buf, size_t size)
{
	mz_stream_raw* raw = (mz_stream_raw*)stream->pUser;
	size_t         bytes_to_read = size;
	size_t         read = 0;

	if (raw->max_total_in > 0)
	{
		if ((int64_t)bytes_to_read > (raw->max_total_in - raw->total_in))
			bytes_to_read = (int32_t)(raw->max_total_in - raw->total_in);
	}

	read = fsReadFromStream(stream->pBase, buf, bytes_to_read);

	if (read > 0)
	{
		raw->total_in += read;
		raw->total_out += read;
	}

	return read;
}

size_t mz_stream_raw_write(FileStream* stream, const void* buf, size_t size)
{
	mz_stream_raw* raw = (mz_stream_raw*)stream->pUser;
	size_t         written = 0;

	written = fsWriteToStream(stream->pBase, buf, size);

	if (written > 0)
	{
		raw->total_out += written;
		raw->total_in += written;
	}

	return written;
}

ssize_t mz_stream_raw_tell(const FileStream* stream) { return fsGetStreamSeekPosition(stream->pBase); }

bool mz_stream_raw_seek(FileStream* stream, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	return fsSeekStream(stream->pBase, baseOffset, seekOffset);
}

bool mz_stream_raw_close(FileStream* stream)
{
	mz_stream_raw* raw = NULL;
	if (stream == NULL)
		return false;
	raw = (mz_stream_raw*)stream->pUser;
	if (!raw)
		return false;
	else
		MZ_FREE(raw);
	memset(stream, 0, sizeof(*stream));
	return true;
}

bool mz_stream_raw_flush(FileStream* stream) { return fsFlushStream(stream->pBase); }

//bool mz_stream_raw_error(void *stream) {
//	mz_stream_raw *raw = (mz_stream_raw *)stream;
//	return mz_stream_error(raw->stream.base);
//}

bool mz_stream_raw_get_prop_int64(FileStream* stream, int32_t prop, int64_t* value)
{
	mz_stream_raw* raw = (mz_stream_raw*)stream->pUser;
	switch (prop)
	{
		case MZ_STREAM_PROP_TOTAL_IN: *value = raw->total_in; return true;
		case MZ_STREAM_PROP_TOTAL_OUT: *value = raw->total_out; return true;
	}
	return false;
}

bool mz_stream_raw_set_prop_int64(FileStream* stream, int32_t prop, int64_t value)
{
	mz_stream_raw* raw = (mz_stream_raw*)stream->pUser;
	switch (prop)
	{
		case MZ_STREAM_PROP_TOTAL_IN_MAX: raw->max_total_in = value; return true;
	}
	return false;
}

/***************************************************************************/

//void *mz_stream_raw_create(void **stream) {
//	mz_stream_raw *raw = NULL;
//
//	raw = (mz_stream_raw *)MZ_ALLOC(sizeof(mz_stream_raw));
//	if (raw != NULL) {
//		memset(raw, 0, sizeof(mz_stream_raw));
//		raw->stream.vtbl = &mz_stream_raw_vtbl;
//	}
//	if (stream != NULL)
//		*stream = raw;
//
//	return raw;
//}
//
//void mz_stream_raw_delete(void **stream) {
//	mz_stream_raw *raw = NULL;
//	if (stream == NULL)
//		return;
//	raw = (mz_stream_raw *)*stream;
//	if (raw != NULL)
//		MZ_FREE(raw);
//	*stream = NULL;
//}
