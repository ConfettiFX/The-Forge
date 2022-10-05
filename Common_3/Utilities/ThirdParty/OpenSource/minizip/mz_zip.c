/* zip.c -- Zip manipulation
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng
   Copyright (C) 2009-2010 Mathias Svensson
     Modifications for Zip64 support
     http://result42.com
   Copyright (C) 2007-2008 Even Rouault
     Modifications of Unzip for Zip64
   Copyright (C) 1998-2010 Gilles Vollant
     https://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_crypt.h"
#include "mz_strm.h"
#ifdef HAVE_BZIP2
#include "mz_strm_bzip.h"
#endif
#ifdef HAVE_LIBCOMP
#include "mz_strm_libcomp.h"
#endif
#ifdef HAVE_LZMA
#include "mz_strm_lzma.h"
#endif
#ifdef HAVE_PKCRYPT
#include "mz_strm_pkcrypt.h"
#endif
#ifdef HAVE_WZAES
#include "mz_strm_wzaes.h"
#endif
#ifdef HAVE_ZLIB
#include "mz_strm_zlib.h"
#endif
#ifdef HAVE_ZSTD
#include "mz_strm_zstd.h"
#endif

#include "mz_zip.h"

#include <ctype.h> /* tolower */
#include <stdio.h> /* snprintf */

#include "../../../Interfaces/IFileSystem.h"    // File streams
#include "../../../Interfaces/ILog.h"           // Assert
#include "../../../Interfaces/IMemory.h"        // allocations

#if defined(_MSC_VER) || defined(__MINGW32__)
#define localtime_r(t1, t2) (localtime_s(t2, t1) == 0 ? t1 : NULL)
#endif
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define snprintf _snprintf
#endif

/***************************************************************************/

static const uint32_t mzZipMagicLocalHeader = 0x04034b50;
static const uint16_t mzCompressMethodAes = MZ_COMPRESS_METHOD_AES;
static const uint32_t mzZipMagicCentralheader = 0x02014b50;
static const uint32_t mzZipMagicDatadescriptor = 0x08074b50;
static const uint32_t mzZipMagicEndheader64 = 0x06064b50;
static const uint32_t mzZipMagicEndlocalheader64 = 0x07064b50;
static const uint32_t mzZipMagicEndheader = 0x06054b50;

#define MZ_ZIP_MAGIC_LOCALHEADERU8 \
	{                              \
		0x50, 0x4b, 0x03, 0x04     \
	}
#define MZ_ZIP_MAGIC_CENTRALHEADERU8 \
	{                                \
		0x50, 0x4b, 0x01, 0x02       \
	}
#define MZ_ZIP_MAGIC_ENDHEADERU8 \
	{                            \
		0x50, 0x4b, 0x05, 0x06   \
	}
#define MZ_ZIP_MAGIC_DATADESCRIPTORU8 \
	{                                 \
		0x50, 0x4b, 0x07, 0x08        \
	}

#define MZ_ZIP_SIZE_LD_ITEM (30)
#define MZ_ZIP_SIZE_CD_ITEM (46)
#define MZ_ZIP_SIZE_CD_LOCATOR64 (20)
#define MZ_ZIP_SIZE_MAX_DATA_DESCRIPTOR (24)

#define MZ_ZIP_OFFSET_CRC_SIZES (14)
#define MZ_ZIP_UNCOMPR_SIZE64_CUSHION (2 * 1024 * 1024)

#ifndef MZ_ZIP_EOCD_MAX_BACK
#define MZ_ZIP_EOCD_MAX_BACK (1 << 20)
#endif

static const uint8_t  u8_zero = 0;
static const uint16_t u16_zero = 0;
static const uint16_t u16_one = 0x1;
static const uint16_t u16_max = UINT16_MAX;
static const uint32_t u32_zero = 0;
static const uint32_t u32_max = UINT32_MAX;
static const int64_t  i64_zero = 0;

/***************************************************************************/

typedef struct mz_zip_s
{
	mz_zip_file file_info;
	mz_zip_file local_file_info;

	FileStream* stream;                 /* main stream */
	FileStream* cd_stream;              /* pointer to the stream with the cd */
	FileStream  cd_mem_stream;          /* memory stream for central directory */
	FileStream  compress_stream;        /* compression stream */
	FileStream  crypt_stream;           /* encryption stream */
	FileStream  file_info_stream;       /* memory stream for storing file info */
	FileStream  local_file_info_stream; /* memory stream for storing local file info */

	FileMode open_mode;
	FileMode entry_open_mode;
	uint8_t  recover;
	uint8_t  data_descriptor;

	uint32_t disk_number_with_cd; /* number of the disk with the central dir */
	int64_t  disk_offset_shift;   /* correction for zips that have wrong offset start of cd */

	int64_t  cd_start_pos;     /* pos of the first file in the central dir stream */
	int64_t  cd_current_pos;   /* pos of the current file in the central dir */
	uint64_t cd_current_entry; /* index of the current file in the central dir*/
	int64_t  cd_offset;        /* offset of start of central directory */
	int64_t  cd_size;          /* size of the central directory */
	uint32_t cd_signature;     /* signature of central directory */

	uint8_t  entry_scanned; /* entry header information read ok */
	uint8_t  entry_opened;  /* entry is open for read/write */
	uint8_t  entry_raw;     /* entry opened with raw mode */
	uint32_t entry_crc32;   /* entry crc32  */

	uint64_t number_entry;

	uint16_t version_madeby;
	char*    comment;
} mz_zip;

static void mz_zip_reset(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;
	memset(zip, 0, sizeof(mz_zip));
	zip->data_descriptor = 1;
}

/***************************************************************************/

#if 0
#define mz_zip_print printf
#else
#define mz_zip_print(fmt, ...)
#endif

/***************************************************************************/

/* Locate the end of central directory */
static bool mz_zip_search_eocd(FileStream* pStream, ssize_t* central_pos)
{
	int64_t file_size = 0;
	int64_t max_back = MZ_ZIP_EOCD_MAX_BACK;
	uint8_t find[4] = MZ_ZIP_MAGIC_ENDHEADERU8;

	if (!fsSeekStream(pStream, SBO_END_OF_FILE, 0))
		return false;

	file_size = fsGetStreamSeekPosition(pStream);

	if (max_back <= 0 || max_back > file_size)
		max_back = file_size;

	return fsFindReverseStream(pStream, (const void*)find, sizeof(find), max_back, central_pos);
}

/* Locate the end of central directory 64 of a zip file */
static bool mz_zip_search_zip64_eocd(FileStream* pStream, const int64_t end_central_offset, int64_t* central_pos)
{
	int64_t  offset = 0;
	uint32_t value32 = 0;
	bool     noerr = true;

	*central_pos = 0;

	/* Zip64 end of central directory locator */
	noerr = fsSeekStream(pStream, SBO_START_OF_FILE, end_central_offset - MZ_ZIP_SIZE_CD_LOCATOR64);
	/* Read locator signature */
	if (noerr)
	{
		noerr = fsReadFromStream(pStream, &value32, sizeof(value32)) == sizeof(value32);
		if (value32 != mzZipMagicEndlocalheader64)
			noerr = false;
	}
	/* Number of the disk with the start of the zip64 end of  central directory */
	if (noerr)
		noerr = fsReadFromStream(pStream, &value32, sizeof(value32)) == sizeof(value32);
	/* Relative offset of the zip64 end of central directory record8 */
	if (noerr)
		noerr = fsReadFromStream(pStream, &offset, sizeof(offset)) == sizeof(offset);
	/* Total number of disks */
	if (noerr)
		noerr = fsReadFromStream(pStream, &value32, sizeof(value32)) == sizeof(value32);
	/* Goto end of central directory record */
	if (noerr)
		noerr = fsSeekStream(pStream, SBO_START_OF_FILE, (ssize_t)offset);
	/* The signature */
	if (noerr)
	{
		noerr = fsReadFromStream(pStream, &value32, sizeof(value32)) == sizeof(value32);
		noerr = value32 != mzZipMagicEndheader64;
	}

	if (noerr)
		*central_pos = offset;

	return noerr;
}

#ifdef HAVE_PKCRYPT
/* Get PKWARE traditional encryption verifier */
static uint16_t mz_zip_get_pk_verify(uint32_t dos_date, uint64_t crc, uint16_t flag)
{
	/* Info-ZIP modification to ZipCrypto format: if bit 3 of the general
     * purpose bit flag is set, it uses high byte of 16-bit File Time. */
	if (flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR)
		return ((dos_date >> 16) & 0xff) << 8 | ((dos_date >> 8) & 0xff);
	return ((crc >> 16) & 0xff) << 8 | ((crc >> 24) & 0xff);
}
#endif

/* Get info about the current file in the zip file */
static bool mz_zip_entry_read_header(FileStream* stream, uint8_t local, mz_zip_file* file_info, FileStream* file_extra_stream)
{
	uint64_t ntfs_time = 0;
	uint32_t reserved = 0;
	uint32_t magic = 0;
	uint32_t dos_date = 0;
	uint32_t field_pos = 0;
	uint16_t field_type = 0;
	uint16_t field_length = 0;
	uint32_t field_length_read = 0;
	uint16_t ntfs_attrib_id = 0;
	uint16_t ntfs_attrib_size = 0;
	uint16_t linkname_size;
	uint16_t value16 = 0;
	uint32_t value32 = 0;
	int64_t  extrafield_pos = 0;
	int64_t  comment_pos = 0;
	int64_t  linkname_pos = 0;
	int64_t  saved_pos = 0;
	//int32_t err = MZ_OK;
	bool  noerr = true;
	char* linkname = NULL;

	memset(file_info, 0, sizeof(mz_zip_file));

	/* Check the magic */
	noerr = fsReadFromStream(stream, &magic, sizeof(magic)) == sizeof(magic);
	if (noerr)
	{
		noerr = !(magic == mzZipMagicEndheader || magic == mzZipMagicEndheader64) && !((local) && (magic != mzZipMagicLocalHeader)) &&
				!((!local) && (magic != mzZipMagicCentralheader));
	}
	//if (err == MZ_END_OF_STREAM)
	//    err = MZ_END_OF_LIST;
	//else if (magic == mzZipMagicEndheader || magic == mzZipMagicEndheader64)
	//    err = MZ_END_OF_LIST;
	//else if ((local) && (magic != mzZipMagicLocalHeader))
	//    err = MZ_FORMAT_ERROR;
	//else if ((!local) && (magic != mzZipMagicCentralheader))
	//    err = MZ_FORMAT_ERROR;

	/* Read header fields */
	if (noerr)
	{
		if (!local)
			noerr = fsReadFromStream(stream, &file_info->version_madeby, sizeof(file_info->version_madeby)) ==
					sizeof(file_info->version_madeby);
		if (noerr)
			noerr = fsReadFromStream(stream, &file_info->version_needed, sizeof(file_info->version_needed)) ==
					sizeof(file_info->version_needed);
		if (noerr)
			noerr = fsReadFromStream(stream, &file_info->flag, sizeof(file_info->flag)) == sizeof(file_info->flag);
		if (noerr)
			noerr = fsReadFromStream(stream, &file_info->compression_method, sizeof(file_info->compression_method)) ==
					sizeof(file_info->compression_method);
		if (noerr)
		{
			noerr = fsReadFromStream(stream, &dos_date, sizeof(dos_date)) == sizeof(dos_date);
			file_info->modified_date = mz_zip_dosdate_to_time_t(dos_date);
		}
		if (noerr)
			noerr = fsReadFromStream(stream, &file_info->crc, sizeof(file_info->crc)) == sizeof(file_info->crc);
#ifdef HAVE_PKCRYPT
		if (noerr && file_info->flag & MZ_ZIP_FLAG_ENCRYPTED)
		{
			/* Use dos_date from header instead of derived from time in zip extensions */
			file_info->pk_verify = mz_zip_get_pk_verify(dos_date, file_info->crc, file_info->flag);
		}
#endif
		if (noerr)
		{
			noerr = fsReadFromStream(stream, &value32, sizeof(value32)) == sizeof(value32);
			file_info->compressed_size = value32;
		}
		if (noerr)
		{
			noerr = fsReadFromStream(stream, &value32, sizeof(value32)) == sizeof(value32);
			file_info->uncompressed_size = value32;
		}
		if (noerr)
			noerr =
				fsReadFromStream(stream, &file_info->filename_size, sizeof(file_info->filename_size)) == sizeof(file_info->filename_size);
		if (noerr)
			noerr = fsReadFromStream(stream, &file_info->extrafield_size, sizeof(file_info->extrafield_size)) ==
					sizeof(file_info->extrafield_size);
		if (!local)
		{
			if (noerr)
				noerr =
					fsReadFromStream(stream, &file_info->comment_size, sizeof(file_info->comment_size)) == sizeof(file_info->comment_size);
			if (noerr)
			{
				noerr = fsReadFromStream(stream, &value16, sizeof(value16)) == sizeof(value16);
				file_info->disk_number = value16;
			}
			if (noerr)
				noerr = fsReadFromStream(stream, &file_info->internal_fa, sizeof(file_info->internal_fa)) == sizeof(file_info->internal_fa);
			if (noerr)
				noerr = fsReadFromStream(stream, &file_info->external_fa, sizeof(file_info->external_fa)) == sizeof(file_info->external_fa);
			if (noerr)
			{
				noerr = fsReadFromStream(stream, &value32, sizeof(value32)) == sizeof(value32);
				file_info->disk_offset = value32;
			}
		}
	}

	if (noerr)
		noerr = fsSeekStream(file_extra_stream, SBO_START_OF_FILE, 0);

	/* Copy variable length data to memory stream for later retrieval */
	if (noerr && (file_info->filename_size > 0))
		noerr = fsCopyStream(file_extra_stream, stream, file_info->filename_size);
	fsWriteToStream(file_extra_stream, &u8_zero, sizeof(u8_zero));
	extrafield_pos = fsGetStreamSeekPosition(file_extra_stream);

	if (noerr && (file_info->extrafield_size > 0))
		noerr = fsCopyStream(file_extra_stream, stream, file_info->extrafield_size);
	fsWriteToStream(file_extra_stream, &u8_zero, sizeof(u8_zero));

	comment_pos = fsGetStreamSeekPosition(file_extra_stream);
	if (noerr && (file_info->comment_size > 0))
		noerr = fsCopyStream(file_extra_stream, stream, file_info->comment_size);
	fsWriteToStream(file_extra_stream, &u8_zero, sizeof(u8_zero));

	linkname_pos = fsGetStreamSeekPosition(file_extra_stream);
	/* Overwrite if we encounter UNIX1 extra block */
	fsWriteToStream(file_extra_stream, &u8_zero, sizeof(u8_zero));

	if (noerr && (file_info->extrafield_size > 0))
	{
		/* Seek to and parse the extra field */
		noerr = fsSeekStream(file_extra_stream, SBO_START_OF_FILE, extrafield_pos);

		while (noerr && (field_pos + 4 <= file_info->extrafield_size))
		{
			noerr = mz_zip_extrafield_read(file_extra_stream, &field_type, &field_length);
			if (!noerr)
				break;
			field_pos += 4;

			/* Don't allow field length to exceed size of remaining extrafield */
			if (field_length > (file_info->extrafield_size - field_pos))
				field_length = (uint16_t)(file_info->extrafield_size - field_pos);

			/* Read ZIP64 extra field */
			if ((field_type == MZ_ZIP_EXTENSION_ZIP64) && (field_length >= 8))
			{
				if (noerr && (file_info->uncompressed_size == UINT32_MAX))
				{
					noerr = fsReadFromStream(file_extra_stream, &file_info->uncompressed_size, sizeof(file_info->uncompressed_size)) ==
							sizeof(file_info->uncompressed_size);
					if (file_info->uncompressed_size < 0)
						noerr = false;
				}
				if (noerr && (file_info->compressed_size == UINT32_MAX))
				{
					noerr = fsReadFromStream(file_extra_stream, &file_info->compressed_size, sizeof(file_info->compressed_size)) ==
							sizeof(file_info->compressed_size);
					if (file_info->compressed_size < 0)
						noerr = false;
				}
				if (noerr && (file_info->disk_offset == UINT32_MAX))
				{
					noerr = fsReadFromStream(file_extra_stream, &file_info->disk_offset, sizeof(file_info->disk_offset)) ==
							sizeof(file_info->disk_offset);
					if (file_info->disk_offset < 0)
						noerr = false;
				}
				if (noerr && (file_info->disk_number == UINT16_MAX))
					noerr = fsReadFromStream(file_extra_stream, &file_info->disk_number, sizeof(file_info->disk_number)) ==
							sizeof(file_info->disk_number);
			}
			/* Read NTFS extra field */
			else if ((field_type == MZ_ZIP_EXTENSION_NTFS) && (field_length > 4))
			{
				if (noerr)
					noerr = fsReadFromStream(file_extra_stream, &reserved, sizeof(reserved)) == sizeof(reserved);
				field_length_read = 4;

				while (noerr && (field_length_read + 4 <= field_length))
				{
					noerr = fsReadFromStream(file_extra_stream, &ntfs_attrib_id, sizeof(ntfs_attrib_id)) == sizeof(ntfs_attrib_id);
					if (noerr)
						noerr =
							fsReadFromStream(file_extra_stream, &ntfs_attrib_size, sizeof(ntfs_attrib_size)) == sizeof(ntfs_attrib_size);
					field_length_read += 4;

					if (noerr && (ntfs_attrib_id == 0x01) && (ntfs_attrib_size == 24))
					{
						noerr = fsReadFromStream(file_extra_stream, &ntfs_time, sizeof(ntfs_time)) == sizeof(ntfs_time);
						mz_zip_ntfs_to_unix_time(ntfs_time, &file_info->modified_date);

						if (noerr)
						{
							noerr = fsReadFromStream(file_extra_stream, &ntfs_time, sizeof(ntfs_time)) == sizeof(ntfs_time);
							mz_zip_ntfs_to_unix_time(ntfs_time, &file_info->accessed_date);
						}
						if (noerr)
						{
							noerr = fsReadFromStream(file_extra_stream, &ntfs_time, sizeof(ntfs_time)) == sizeof(ntfs_time);
							mz_zip_ntfs_to_unix_time(ntfs_time, &file_info->creation_date);
						}
					}
					else if (noerr && (field_length_read + ntfs_attrib_size <= field_length))
					{
						noerr = fsSeekStream(file_extra_stream, SBO_CURRENT_POSITION, ntfs_attrib_size);
					}

					field_length_read += ntfs_attrib_size;
				}
			}
			/* Read UNIX1 extra field */
			else if ((field_type == MZ_ZIP_EXTENSION_UNIX1) && (field_length >= 12))
			{
				if (noerr)
				{
					noerr = fsReadFromStream(file_extra_stream, &value32, sizeof(value32)) == sizeof(value32);
					if (noerr && file_info->accessed_date == 0)
						file_info->accessed_date = value32;
				}
				if (noerr)
				{
					noerr = fsReadFromStream(file_extra_stream, &value32, sizeof(value32)) == sizeof(value32);
					if (noerr && file_info->modified_date == 0)
						file_info->modified_date = value32;
				}
				if (noerr)
					noerr = fsReadFromStream(file_extra_stream, &value16, sizeof(value16)) == sizeof(value16); /* User id */
				if (noerr)
					noerr = fsReadFromStream(file_extra_stream, &value16, sizeof(value16)) == sizeof(value16); /* Group id */

				/* Copy linkname to end of file extra stream so we can return null
                   terminated string */
				linkname_size = field_length - 12;
				if ((noerr) && (linkname_size > 0))
				{
					linkname = (char*)MZ_ALLOC(linkname_size);
					if (linkname != NULL)
					{
						if (fsReadFromStream(file_extra_stream, linkname, linkname_size) != linkname_size)
							noerr = false;
						if (noerr)
						{
							saved_pos = fsGetStreamSeekPosition(file_extra_stream);

							fsSeekStream(file_extra_stream, SBO_START_OF_FILE, linkname_pos);
							fsWriteToStream(file_extra_stream, linkname, linkname_size);
							fsWriteToStream(file_extra_stream, &u8_zero, sizeof(u8_zero));

							fsSeekStream(file_extra_stream, SBO_START_OF_FILE, saved_pos);
						}
						MZ_FREE(linkname);
					}
				}
			}
#ifdef HAVE_WZAES
			/* Read AES extra field */
			else if ((field_type == MZ_ZIP_EXTENSION_AES) && (field_length == 7))
			{
				uint8_t value8 = 0;
				/* Verify version info */
				noerr = fsReadFromStream(file_extra_stream, &value16, sizeof(value16)) == sizeof(value16);
				/* Support AE-1 and AE-2 */
				if (value16 != 1 && value16 != 2)
					noerr = false;
				file_info->aes_version = value16;
				if (noerr)
					noerr = fsReadFromStream(file_extra_stream, &value8, sizeof(value8)) == sizeof(value8);
				if ((char)value8 != 'A')
					noerr = false;
				if (noerr)
					noerr = fsReadFromStream(file_extra_stream, &value8, sizeof(value8)) == sizeof(value8);
				if ((char)value8 != 'E')
					noerr = false;
				/* Get AES encryption strength and actual compression method */
				if (noerr)
				{
					noerr = fsReadFromStream(file_extra_stream, &value8, sizeof(value8)) == sizeof(value8);
					file_info->aes_encryption_mode = value8;
				}
				if (noerr)
				{
					noerr = fsReadFromStream(file_extra_stream, &value16, sizeof(value16)) == sizeof(value16);
					file_info->compression_method = value16;
				}
			}
#endif
			else if (field_length > 0)
			{
				noerr = fsSeekStream(file_extra_stream, SBO_CURRENT_POSITION, field_length);
			}

			field_pos += field_length;
		}
	}

	/* Get pointers to variable length data */
	ASSERT(fsIsMemoryStream(file_extra_stream));
	fsGetMemoryStreamBuffer(file_extra_stream, (const void**)&file_info->filename);
	fsGetMemoryStreamBufferAt(file_extra_stream, extrafield_pos, (const void**)&file_info->extrafield);
	fsGetMemoryStreamBufferAt(file_extra_stream, comment_pos, (const void**)&file_info->comment);
	fsGetMemoryStreamBufferAt(file_extra_stream, linkname_pos, (const void**)&file_info->linkname);

	/* Set to empty string just in-case */
	if (file_info->filename == NULL)
		file_info->filename = "";
	if (file_info->extrafield == NULL)
		file_info->extrafield_size = 0;
	if (file_info->comment == NULL)
		file_info->comment = "";
	if (file_info->linkname == NULL)
		file_info->linkname = "";

	if (noerr)
	{
		mz_zip_print("Zip - Entry - Read header - %s (local %" PRId8 ")\n", file_info->filename, local);
		mz_zip_print(
			"Zip - Entry - Read header compress (ucs %" PRId64 " cs %" PRId64 " crc 0x%08" PRIx32 ")\n", file_info->uncompressed_size,
			file_info->compressed_size, file_info->crc);
		if (!local)
		{
			mz_zip_print(
				"Zip - Entry - Read header disk (disk %" PRIu32 " offset %" PRId64 ")\n", file_info->disk_number, file_info->disk_offset);
		}
		mz_zip_print(
			"Zip - Entry - Read header variable (fnl %" PRId32 " efs %" PRId32 " cms %" PRId32 ")\n", file_info->filename_size,
			file_info->extrafield_size, file_info->comment_size);
	}

	return noerr;
}

static bool
	mz_zip_entry_read_descriptor(FileStream* stream, uint8_t zip64, uint32_t* crc32, int64_t* compressed_size, int64_t* uncompressed_size)
{
	uint32_t value32 = 0;
	int64_t  value64 = 0;
	int32_t  noerr = true;

	noerr = fsReadFromStream(stream, &value32, sizeof(value32)) == sizeof(value32);
	if (value32 != mzZipMagicDatadescriptor)
		noerr = false;
	if (noerr)
		noerr = fsReadFromStream(stream, &value32, sizeof(value32)) == sizeof(value32);
	if (noerr && (crc32 != NULL))
		*crc32 = value32;
	if (noerr)
	{
		/* If zip 64 extension is enabled then read as 8 byte */
		if (!zip64)
		{
			noerr = fsReadFromStream(stream, &value32, sizeof(value32)) == sizeof(value32);
			value64 = value32;
		}
		else
		{
			noerr = fsReadFromStream(stream, &value64, sizeof(value64)) == sizeof(value64);
			if (value64 < 0)
				noerr = false;
		}
		if ((noerr) && (compressed_size != NULL))
			*compressed_size = value64;
	}
	if (noerr)
	{
		if (!zip64)
		{
			noerr = fsReadFromStream(stream, &value32, sizeof(value32)) == sizeof(value32);
			value64 = value32;
		}
		else
		{
			noerr = fsReadFromStream(stream, &value64, sizeof(value64)) == sizeof(value64);
			if (value64 < 0)
				noerr = false;
		}
		if ((noerr) && (uncompressed_size != NULL))
			*uncompressed_size = value64;
	}

	return noerr;
}

static bool mz_zip_entry_write_crc_sizes(FileStream* stream, uint8_t zip64, uint8_t mask, mz_zip_file* file_info)
{
	bool noerr = true;

	static const uint32_t u32_zero = 0;

	if (mask)
		noerr = fsWriteToStream(stream, &u32_zero, sizeof(u32_zero)) == sizeof(u32_zero);
	else
		noerr = fsWriteToStream(stream, &file_info->crc, sizeof(file_info->crc)) == sizeof(file_info->crc); /* crc */

	/* For backwards-compatibility with older zip applications we set all sizes to UINT32_MAX
     * when zip64 is needed, instead of only setting sizes larger than UINT32_MAX. */

	if (noerr)
	{
		if (zip64) /* compr size */
			noerr = fsWriteToStream(stream, &u32_max, sizeof(u32_max)) == sizeof(u32_max);
		else
		{
			uint32_t tmp = (uint32_t)file_info->compressed_size;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}
	if (noerr)
	{
		if (mask) /* uncompr size */
			noerr = fsWriteToStream(stream, &u32_zero, sizeof(u32_zero)) == sizeof(u32_zero);
		else if (zip64)
			noerr = fsWriteToStream(stream, &u32_max, sizeof(u32_max)) == sizeof(u32_max);
		else
		{
			uint32_t tmp = (uint32_t)file_info->uncompressed_size;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}
	return noerr;
}

static bool mz_zip_entry_needs_zip64(mz_zip_file* file_info, uint8_t local, uint8_t* zip64)
{
	uint32_t max_uncompressed_size = UINT32_MAX;
	uint8_t  needs_zip64 = 0;

	if (zip64 == NULL)
		return false;

	*zip64 = 0;

	if (local)
	{
		/* At local header we might not know yet whether compressed size will overflow unsigned
           32-bit integer which might happen for high entropy data so we give it some cushion */

		max_uncompressed_size -= MZ_ZIP_UNCOMPR_SIZE64_CUSHION;
	}

	needs_zip64 = (file_info->uncompressed_size >= max_uncompressed_size) || (file_info->compressed_size >= UINT32_MAX);

	if (!local)
	{
		/* Disk offset and number only used in central directory header */
		needs_zip64 |= (file_info->disk_offset >= UINT32_MAX) || (file_info->disk_number >= UINT16_MAX);
	}

	if (file_info->zip64 == MZ_ZIP64_AUTO)
	{
		/* If uncompressed size is unknown, assume zip64 for 64-bit data descriptors */
		if (local && file_info->uncompressed_size == 0)
		{
			/* Don't use zip64 for local header directory entries */
			if (!mz_zip_attrib_is_dir(file_info->external_fa, file_info->version_madeby))
			{
				*zip64 = 1;
			}
		}
		*zip64 |= needs_zip64;
	}
	else if (file_info->zip64 == MZ_ZIP64_FORCE)
	{
		*zip64 = 1;
	}
	else if (file_info->zip64 == MZ_ZIP64_DISABLE)
	{
		/* Zip64 extension is required to zip file */
		if (needs_zip64)
			return false;
	}

	return true;
}

static bool mz_zip_entry_write_header(FileStream* stream, uint8_t local, mz_zip_file* file_info)
{
	uint64_t    ntfs_time = 0;
	uint32_t    reserved = 0;
	uint32_t    dos_date = 0;
	uint16_t    extrafield_size = 0;
	uint16_t    field_type = 0;
	uint16_t    field_length = 0;
	uint16_t    field_length_zip64 = 0;
	uint16_t    field_length_ntfs = 0;
	uint16_t    field_length_aes = 0;
	uint16_t    field_length_unix1 = 0;
	uint16_t    filename_size = 0;
	uint16_t    filename_length = 0;
	uint16_t    linkname_size = 0;
	uint16_t    version_needed = 0;
	int32_t     comment_size = 0;
	bool        noerr = true;
	bool        noerr_mem = true;
	uint8_t     zip64 = 0;
	uint8_t     skip_aes = 0;
	uint8_t     mask = 0;
	uint8_t     write_end_slash = 0;
	const char* filename = NULL;
	char        masked_name[64];
	FileStream  file_extra_stream = (FileStream){ 0 };
	FileStream* pFileExtraStream = &file_extra_stream;

	if (file_info == NULL)
		return false;

	if ((local) && (file_info->flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO))
		mask = 1;

	/* Determine if zip64 extra field is necessary */
	noerr = mz_zip_entry_needs_zip64(file_info, local, &zip64);
	if (!noerr)
		return false;

	/* Start calculating extra field sizes */
	if (zip64)
	{
		/* Both compressed and uncompressed sizes must be included (at least in local header) */
		field_length_zip64 = 8 + 8;
		if ((!local) && (file_info->disk_offset >= UINT32_MAX))
			field_length_zip64 += 8;

		extrafield_size += 4;
		extrafield_size += field_length_zip64;
	}

	/* Calculate extra field size and check for duplicates */
	if (file_info->extrafield_size > 0)
	{
		fsOpenStreamFromMemory(file_info->extrafield, file_info->extrafield_size, FM_READ, false, pFileExtraStream);

		do
		{
			noerr_mem = fsReadFromStream(pFileExtraStream, &field_type, sizeof(field_type)) == sizeof(field_type);
			if (noerr_mem)
				noerr_mem = fsReadFromStream(pFileExtraStream, &field_length, sizeof(field_length)) == sizeof(field_length);
			if (!noerr_mem)
				break;

			/* Prefer incoming aes extensions over ours */
			if (field_type == MZ_ZIP_EXTENSION_AES)
				skip_aes = 1;

			/* Prefer our zip64, ntfs, unix1 extension over incoming */
			if (field_type != MZ_ZIP_EXTENSION_ZIP64 && field_type != MZ_ZIP_EXTENSION_NTFS && field_type != MZ_ZIP_EXTENSION_UNIX1)
				extrafield_size += 4 + field_length;

			if (noerr_mem)
				noerr_mem = fsSeekStream(pFileExtraStream, SBO_CURRENT_POSITION, field_length);
		} while (noerr_mem);
	}

#ifdef HAVE_WZAES
	if (!skip_aes)
	{
		if ((file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version))
		{
			field_length_aes = 1 + 1 + 1 + 2 + 2;
			extrafield_size += 4 + field_length_aes;
		}
	}
#else
	MZ_UNUSED(field_length_aes);
	MZ_UNUSED(skip_aes);
#endif
	/* NTFS timestamps */
	if ((file_info->modified_date != 0) && (file_info->accessed_date != 0) && (file_info->creation_date != 0) && (!mask))
	{
		field_length_ntfs = 8 + 8 + 8 + 4 + 2 + 2;
		extrafield_size += 4 + field_length_ntfs;
	}

	/* Unix1 symbolic links */
	if (file_info->linkname != NULL && *file_info->linkname != 0)
	{
		linkname_size = (uint16_t)strlen(file_info->linkname);
		field_length_unix1 = 12 + linkname_size;
		extrafield_size += 4 + field_length_unix1;
	}

	if (local)
		noerr = fsWriteToStream(stream, &mzZipMagicLocalHeader, sizeof(mzZipMagicLocalHeader)) == sizeof(mzZipMagicLocalHeader);
	else
	{
		noerr = fsWriteToStream(stream, &mzZipMagicCentralheader, sizeof(mzZipMagicCentralheader)) == sizeof(mzZipMagicCentralheader);
		if (noerr)
			noerr =
				fsWriteToStream(stream, &file_info->version_madeby, sizeof(file_info->version_madeby)) == sizeof(file_info->version_madeby);
	}

	/* Calculate version needed to extract */
	if (noerr)
	{
		version_needed = file_info->version_needed;
		if (version_needed == 0)
		{
			version_needed = 20;
			if (zip64)
				version_needed = 45;
#ifdef HAVE_WZAES
			if ((file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version))
				version_needed = 51;
#endif
#if defined(HAVE_LZMA) || defined(HAVE_LIBCOMP)
			if ((file_info->compression_method == MZ_COMPRESS_METHOD_LZMA) || (file_info->compression_method == MZ_COMPRESS_METHOD_XZ))
				version_needed = 63;
#endif
		}
		noerr = fsWriteToStream(stream, &version_needed, sizeof(version_needed)) == sizeof(version_needed);
	}
	if (noerr)
		noerr = fsWriteToStream(stream, &file_info->flag, sizeof(file_info->flag)) == sizeof(file_info->flag);
	if (noerr)
	{
#ifdef HAVE_WZAES
		if ((file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version))
			noerr = fsWriteToStream(stream, &mzCompressMethodAes, sizeof(mzCompressMethodAes)) == sizeof(mzCompressMethodAes);
		else
#endif
			noerr = fsWriteToStream(stream, &file_info->compression_method, sizeof(file_info->compression_method)) ==
					sizeof(file_info->compression_method);
	}
	if (noerr)
	{
		if (file_info->modified_date != 0 && !mask)
			dos_date = mz_zip_time_t_to_dos_date(file_info->modified_date);
		noerr = fsWriteToStream(stream, &dos_date, sizeof(dos_date)) == sizeof(dos_date);
	}

	if (noerr)
		noerr = mz_zip_entry_write_crc_sizes(stream, zip64, mask, file_info);

	if (mask)
	{
		snprintf(masked_name, sizeof(masked_name), "%" PRIx32 "_%" PRIx64, file_info->disk_number, file_info->disk_offset);
		filename = masked_name;
	}
	else
	{
		filename = file_info->filename;
	}

	filename_length = (uint16_t)strlen(filename);
	filename_size += filename_length;

	if (mz_zip_attrib_is_dir(file_info->external_fa, file_info->version_madeby) &&
		((filename[filename_length - 1] != '/') && (filename[filename_length - 1] != '\\')))
	{
		filename_size += 1;
		write_end_slash = 1;
	}

	if (noerr)
		noerr = fsWriteToStream(stream, &filename_size, sizeof(filename_size)) == sizeof(filename_size);
	if (noerr)
		noerr = fsWriteToStream(stream, &extrafield_size, sizeof(extrafield_size)) == sizeof(extrafield_size);

	if (!local)
	{
		if (file_info->comment != NULL)
		{
			comment_size = (int32_t)strlen(file_info->comment);
			if (comment_size > UINT16_MAX)
				comment_size = UINT16_MAX;
		}
		if (noerr)
		{
			uint16_t tmp = comment_size;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		if (noerr)
		{
			uint16_t tmp = file_info->disk_number;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		if (noerr)
			noerr = fsWriteToStream(stream, &file_info->internal_fa, sizeof(file_info->internal_fa)) == sizeof(file_info->internal_fa);
		if (noerr)
			noerr = fsWriteToStream(stream, &file_info->external_fa, sizeof(file_info->external_fa)) == sizeof(file_info->external_fa);
		if (noerr)
		{
			if (file_info->disk_offset >= UINT32_MAX)
				noerr = fsWriteToStream(stream, &u32_zero, sizeof(u32_zero)) == sizeof(u32_zero);
			else
			{
				uint32_t tmp = (uint32_t)file_info->disk_offset;
				noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
			}
		}
	}

	if (noerr)
	{
		if (fsWriteToStream(stream, filename, filename_length) != filename_length)
			noerr = false;

		static const uint8_t slash = '/';

		/* Ensure that directories have a slash appended to them for compatibility */
		if (noerr && write_end_slash)
			noerr = fsWriteToStream(stream, &slash, sizeof(slash)) == sizeof(slash);
	}

	/* Write ZIP64 extra field first so we can update sizes later if data descriptor not used */
	if (noerr && (zip64))
	{
		noerr = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_ZIP64, field_length_zip64);
		if (noerr)
		{
			if (mask)
				noerr = fsWriteToStream(stream, &i64_zero, sizeof(i64_zero)) == sizeof(i64_zero);
			else
				noerr = fsWriteToStream(stream, &file_info->uncompressed_size, sizeof(file_info->uncompressed_size)) ==
						sizeof(file_info->uncompressed_size);
		}
		if (noerr)
			noerr = fsWriteToStream(stream, &file_info->compressed_size, sizeof(file_info->compressed_size)) ==
					sizeof(file_info->compressed_size);
		if (noerr && (!local) && (file_info->disk_offset >= UINT32_MAX))
			noerr = fsWriteToStream(stream, &file_info->disk_offset, sizeof(file_info->disk_offset)) == sizeof(file_info->disk_offset);
		if (noerr && (!local) && (file_info->disk_number >= UINT16_MAX))
			noerr = fsWriteToStream(stream, &file_info->disk_number, sizeof(file_info->disk_number)) == sizeof(file_info->disk_number);
	}
	/* Write NTFS extra field */
	if ((noerr) && (field_length_ntfs > 0))
	{
		noerr = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_NTFS, field_length_ntfs);
		if (noerr)
			noerr = fsWriteToStream(stream, &reserved, sizeof(reserved)) == sizeof(reserved);
		if (noerr)
			noerr = fsWriteToStream(stream, &u16_one, sizeof(u16_one)) == sizeof(u16_one);
		if (noerr)
		{
			uint16_t tmp = field_length_ntfs - 8;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		};
		if (noerr)
		{
			mz_zip_unix_to_ntfs_time(file_info->modified_date, &ntfs_time);
			noerr = fsWriteToStream(stream, &ntfs_time, sizeof(ntfs_time)) == sizeof(ntfs_time);
		}
		if (noerr)
		{
			mz_zip_unix_to_ntfs_time(file_info->accessed_date, &ntfs_time);
			noerr = fsWriteToStream(stream, &ntfs_time, sizeof(ntfs_time)) == sizeof(ntfs_time);
		}
		if (noerr)
		{
			mz_zip_unix_to_ntfs_time(file_info->creation_date, &ntfs_time);
			noerr = fsWriteToStream(stream, &ntfs_time, sizeof(ntfs_time)) == sizeof(ntfs_time);
		}
	}
	/* Write UNIX extra block extra field */
	if (noerr && (field_length_unix1 > 0))
	{
		noerr = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_UNIX1, field_length_unix1);
		if (noerr)
		{
			uint32_t tmp = (uint32_t)file_info->accessed_date;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		if (noerr)
		{
			uint32_t tmp = (uint32_t)file_info->modified_date;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		if (noerr) /* User id */
			noerr = fsWriteToStream(stream, &u16_zero, sizeof(u16_zero)) == sizeof(u16_zero);
		if (noerr) /* Group id */
			noerr = fsWriteToStream(stream, &u16_zero, sizeof(u16_zero)) == sizeof(u16_zero);
		if (noerr && linkname_size > 0)
		{
			if (fsWriteToStream(stream, file_info->linkname, linkname_size) != linkname_size)
				noerr = false;
		}
	}
#ifdef HAVE_WZAES
	/* Write AES extra field */
	if ((noerr) && (!skip_aes) && (file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version))
	{
		noerr = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_AES, field_length_aes);
		if (noerr)
			noerr = fsWriteToStream(stream, &file_info->aes_version, sizeof(file_info->aes_version)) == sizeof(file_info->aes_version);
		if (noerr)
		{
			uint8_t tmp = 'A';
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		if (noerr)
		{
			uint8_t tmp = 'E';
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		if (noerr)
			noerr = fsWriteToStream(stream, &file_info->aes_encryption_mode, sizeof(file_info->aes_encryption_mode)) ==
					sizeof(file_info->aes_encryption_mode);
		if (noerr)
			noerr = fsWriteToStream(stream, &file_info->compression_method, sizeof(file_info->compression_method)) ==
					sizeof(file_info->compression_method);
	}
#endif

	if (file_info->extrafield_size > 0)
	{
		noerr_mem = fsSeekStream(pFileExtraStream, SBO_START_OF_FILE, 0);
		while (noerr && noerr_mem)
		{
			noerr_mem = fsReadFromStream(pFileExtraStream, &field_type, sizeof(field_type)) == sizeof(field_type);
			if (noerr_mem)
				noerr_mem = fsReadFromStream(pFileExtraStream, &field_length, sizeof(field_length)) == sizeof(field_length);
			if (!noerr_mem)
				break;

			/* Prefer our zip 64, ntfs, unix1 extensions over incoming */
			if (field_type == MZ_ZIP_EXTENSION_ZIP64 || field_type == MZ_ZIP_EXTENSION_NTFS || field_type == MZ_ZIP_EXTENSION_UNIX1)
			{
				noerr_mem = fsSeekStream(pFileExtraStream, SBO_CURRENT_POSITION, field_length);
				continue;
			}

			noerr = fsWriteToStream(stream, &field_type, sizeof(field_type)) == sizeof(field_type);
			if (noerr)
				noerr = fsWriteToStream(stream, &field_length, sizeof(field_length)) == sizeof(field_length);
			if (noerr)
				noerr = fsCopyStream(stream, pFileExtraStream, field_length);
		}

		fsCloseStream(pFileExtraStream);
	}

	if (noerr && (!local) && (file_info->comment != NULL))
	{
		if (fsWriteToStream(stream, file_info->comment, file_info->comment_size) != file_info->comment_size)
			noerr = false;
	}

	return noerr;
}

static bool
	mz_zip_entry_write_descriptor(FileStream* stream, uint8_t zip64, uint32_t crc32, int64_t compressed_size, int64_t uncompressed_size)
{
	bool noerr = true;

	noerr = fsWriteToStream(stream, &mzZipMagicDatadescriptor, sizeof(mzZipMagicDatadescriptor)) == sizeof(mzZipMagicDatadescriptor);
	if (noerr)
		noerr = fsWriteToStream(stream, &crc32, sizeof(crc32)) == sizeof(crc32);

	/* Store data descriptor as 8 bytes if zip 64 extension enabled */
	if (noerr)
	{
		/* Zip 64 extension is enabled when uncompressed size is > UINT32_MAX */
		if (!zip64)
		{
			uint32_t tmp = (uint32_t)compressed_size;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		else
		{
			int64_t tmp = compressed_size;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}
	if (noerr)
	{
		if (!zip64)
		{
			uint32_t tmp = (uint32_t)uncompressed_size;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		else
		{
			int64_t tmp = uncompressed_size;
			noerr = fsWriteToStream(stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}

	return noerr;
}

static bool mz_zip_read_cd(void* handle)
{
	mz_zip*  zip = (mz_zip*)handle;
	uint64_t number_entry_cd64 = 0;
	uint64_t number_entry_cd = 0;
	ssize_t  eocd_pos = 0;
	int64_t  eocd_pos64 = 0;
	int64_t  value64i = 0;
	uint16_t value16 = 0;
	uint32_t value32 = 0;
	uint64_t value64 = 0;
	uint16_t comment_size = 0;
	size_t   comment_read = 0;
	bool     noerr = true;

	if (zip == NULL)
		return false;

	/* Read and cache central directory records */
	noerr = mz_zip_search_eocd(zip->stream, &eocd_pos);
	if (noerr)
	{
		/* The signature, already checked */
		noerr = fsReadFromStream(zip->stream, &value32, sizeof(value32)) == sizeof(value32);
		/* Number of this disk */
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &value16, sizeof(value16)) == sizeof(value16);
		/* Number of the disk with the start of the central directory */
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &value16, sizeof(value16)) == sizeof(value16);
		zip->disk_number_with_cd = value16;
		/* Total number of entries in the central dir on this disk */
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &value16, sizeof(value16)) == sizeof(value16);
		zip->number_entry = value16;
		/* Total number of entries in the central dir */
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &value16, sizeof(value16)) == sizeof(value16);
		number_entry_cd = value16;
		if (number_entry_cd != zip->number_entry)
			noerr = false;
		/* Size of the central directory */
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &value32, sizeof(value32)) == sizeof(value32);
		if (noerr)
			zip->cd_size = value32;
		/* Offset of start of central directory with respect to the starting disk number */
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &value32, sizeof(value32)) == sizeof(value32);
		if (noerr)
			zip->cd_offset = value32;
		/* Zip file global comment length */
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &comment_size, sizeof(comment_size)) == sizeof(comment_size);
		if ((noerr) && (comment_size > 0))
		{
			zip->comment = (char*)MZ_ALLOC(comment_size + 1);
			if (zip->comment != NULL)
			{
				comment_read = fsReadFromStream(zip->stream, zip->comment, comment_size);
				/* Don't fail if incorrect comment length read, not critical */
				if (comment_read < 0)
					comment_read = 0;
				zip->comment[comment_read] = 0;
			}
		}

		if ((noerr) && ((number_entry_cd == UINT16_MAX) || (zip->cd_offset == UINT32_MAX)))
		{
			/* Format should be Zip64, as the central directory or file size is too large */
			if (mz_zip_search_zip64_eocd(zip->stream, eocd_pos, &eocd_pos64))
			{
				eocd_pos = eocd_pos64;

				noerr = fsSeekStream(zip->stream, SBO_START_OF_FILE, eocd_pos);
				/* The signature, already checked */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &value32, sizeof(value32)) == sizeof(value32);
				/* Size of zip64 end of central directory record */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &value64, sizeof(value64)) == sizeof(value64);
				/* Version made by */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &zip->version_madeby, sizeof(zip->version_madeby)) == sizeof(zip->version_madeby);
				/* Version needed to extract */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &value16, sizeof(value16)) == sizeof(value16);
				/* Number of this disk */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &value32, sizeof(value32)) == sizeof(value32);
				/* Number of the disk with the start of the central directory */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &zip->disk_number_with_cd, sizeof(zip->disk_number_with_cd)) ==
							sizeof(zip->disk_number_with_cd);
				/* Total number of entries in the central directory on this disk */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &zip->number_entry, sizeof(zip->number_entry)) == sizeof(zip->number_entry);
				/* Total number of entries in the central directory */
				if (noerr)
					noerr = fsReadFromStream(zip->stream, &number_entry_cd64, sizeof(number_entry_cd64)) == sizeof(number_entry_cd64);
				if (zip->number_entry != number_entry_cd64)
					noerr = false;
				/* Size of the central directory */
				if (noerr)
				{
					noerr = fsReadFromStream(zip->stream, &zip->cd_size, sizeof(zip->cd_size)) == sizeof(zip->cd_size);
					if (zip->cd_size < 0)
						noerr = false;
				}
				/* Offset of start of central directory with respect to the starting disk number */
				if (noerr)
				{
					noerr = fsReadFromStream(zip->stream, &zip->cd_offset, sizeof(zip->cd_offset)) == sizeof(zip->cd_offset);
					if (zip->cd_offset < 0)
						noerr = false;
				}
			}
			else if (
				(zip->number_entry == UINT16_MAX) || (number_entry_cd != zip->number_entry) || (zip->cd_size == UINT16_MAX) ||
				(zip->cd_offset == UINT32_MAX))
			{
				noerr = false;
			}
		}
	}

	if (noerr)
	{
		mz_zip_print(
			"Zip - Read cd (disk %" PRId32 " entries %" PRId64 " offset %" PRId64 " size %" PRId64 ")\n", zip->disk_number_with_cd,
			zip->number_entry, zip->cd_offset, zip->cd_size);

		/* Verify central directory signature exists at offset */
		noerr = fsSeekStream(zip->stream, SBO_START_OF_FILE, zip->cd_offset);
		if (noerr)
			noerr = fsReadFromStream(zip->stream, &zip->cd_signature, sizeof(zip->cd_signature)) == sizeof(zip->cd_signature);
		if ((noerr) && (zip->cd_signature != mzZipMagicCentralheader))
		{
			/* If cd exists in large file and no zip-64 support, error for recover */
			if (eocd_pos > UINT32_MAX && eocd_pos64 == 0)
				noerr = false;
			/* If cd not found attempt to seek backward to find it */
			if (noerr)
				noerr = fsSeekStream(zip->stream, SBO_START_OF_FILE, eocd_pos - zip->cd_size);
			if (noerr)
				noerr = fsReadFromStream(zip->stream, &zip->cd_signature, sizeof(zip->cd_signature)) == sizeof(zip->cd_signature);
			if (noerr && (zip->cd_signature == mzZipMagicCentralheader))
			{
				/* If found compensate for incorrect locations */
				value64i = zip->cd_offset;
				zip->cd_offset = eocd_pos - zip->cd_size;
				/* Assume disk has prepended data */
				zip->disk_offset_shift = zip->cd_offset - value64i;
			}
		}
	}

	if (noerr)
	{
		if (eocd_pos < zip->cd_offset)
		{
			/* End of central dir should always come after central dir */
			noerr = false;
		}
		else if ((uint64_t)eocd_pos < (uint64_t)zip->cd_offset + zip->cd_size)
		{
			/* Truncate size of cd if incorrect size or offset provided */
			zip->cd_size = eocd_pos - zip->cd_offset;
		}
	}

	return noerr;
}

static bool mz_zip_write_cd(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;
	int64_t zip64_eocd_pos_inzip = 0;
	int64_t disk_number = 0;
	int64_t disk_size = 0;
	int32_t comment_size = 0;
	bool    noerr = true;

	if (zip == NULL)
		return false;

	if (fsGetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &disk_number))
		zip->disk_number_with_cd = (uint32_t)disk_number;
	if (fsGetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_SIZE, &disk_size) && disk_size > 0)
		zip->disk_number_with_cd += 1;
	fsSetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, -1);
	if ((zip->disk_number_with_cd > 0) && (zip->open_mode & FM_APPEND))
	{
		// Overwrite existing central directory if using split disks
		fsSeekStream(zip->stream, SBO_START_OF_FILE, 0);
	}

	zip->cd_offset = fsGetStreamSeekPosition(zip->stream);
	fsSeekStream(&zip->cd_mem_stream, SBO_END_OF_FILE, 0);
	zip->cd_size = (uint32_t)fsGetStreamSeekPosition(&zip->cd_mem_stream);
	fsSeekStream(&zip->cd_mem_stream, SBO_START_OF_FILE, 0);

	noerr = fsCopyStream(zip->stream, &zip->cd_mem_stream, (int32_t)zip->cd_size);

	mz_zip_print(
		"Zip - Write cd (disk %" PRId32 " entries %" PRId64 " offset %" PRId64 " size %" PRId64 ")\n", zip->disk_number_with_cd,
		zip->number_entry, zip->cd_offset, zip->cd_size);

	if (zip->cd_size == 0 && zip->number_entry > 0)
	{
		// Zip does not contain central directory, open with recovery option
		return false;
	}

	/* Write the ZIP64 central directory header */
	if (zip->cd_offset >= UINT32_MAX || zip->number_entry > UINT16_MAX)
	{
		zip64_eocd_pos_inzip = fsGetStreamSeekPosition(zip->stream);

		noerr = fsWriteToStream(zip->stream, &mzZipMagicEndheader64, sizeof(mzZipMagicEndheader64)) == sizeof(mzZipMagicEndheader64);

		/* Size of this 'zip64 end of central directory' */
		if (noerr)
		{
			uint64_t tmp = 44;
			noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		/* Version made by */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->version_madeby, sizeof(zip->version_madeby)) == sizeof(zip->version_madeby);
		/* Version needed */
		if (noerr)
		{
			uint16_t tmp = 45;
			noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
		/* Number of this disk */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->disk_number_with_cd, sizeof(zip->disk_number_with_cd)) ==
					sizeof(zip->disk_number_with_cd);
		/* Number of the disk with the start of the central directory */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->disk_number_with_cd, sizeof(zip->disk_number_with_cd)) ==
					sizeof(zip->disk_number_with_cd);
		/* Total number of entries in the central dir on this disk */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->number_entry, sizeof(zip->number_entry)) == sizeof(zip->number_entry);
		/* Total number of entries in the central dir */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->number_entry, sizeof(zip->number_entry)) == sizeof(zip->number_entry);
		/* Size of the central directory */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->cd_size, sizeof(zip->cd_size)) == sizeof(zip->cd_size);
		/* Offset of start of central directory with respect to the starting disk number */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->cd_offset, sizeof(zip->cd_offset)) == sizeof(zip->cd_offset);
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &mzZipMagicEndlocalheader64, sizeof(mzZipMagicEndlocalheader64)) ==
					sizeof(mzZipMagicEndlocalheader64);

		/* Number of the disk with the start of the central directory */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip->disk_number_with_cd, sizeof(zip->disk_number_with_cd)) ==
					sizeof(zip->disk_number_with_cd);
		/* Relative offset to the end of zip64 central directory */
		if (noerr)
			noerr = fsWriteToStream(zip->stream, &zip64_eocd_pos_inzip, sizeof(zip64_eocd_pos_inzip)) == sizeof(zip64_eocd_pos_inzip);
		/* Number of the disk with the start of the central directory */
		if (noerr)
		{
			uint32_t tmp = zip->disk_number_with_cd + 1;
			noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}

	/* Write the central directory header */

	/* Signature */
	if (noerr)
		noerr = fsWriteToStream(zip->stream, &mzZipMagicEndheader, sizeof(mzZipMagicEndheader)) == sizeof(mzZipMagicEndheader);
	/* Number of this disk */
	if (noerr)
	{
		uint16_t tmp = zip->disk_number_with_cd;
		noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
	}
	/* Number of the disk with the start of the central directory */
	if (noerr)
	{
		uint16_t tmp = zip->disk_number_with_cd;
		noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
	}
	/* Total number of entries in the central dir on this disk */
	if (noerr)
	{
		if (zip->number_entry >= UINT16_MAX)
			noerr = fsWriteToStream(zip->stream, &u16_max, sizeof(u16_max)) == sizeof(u16_max);
		else
		{
			uint16_t tmp = (uint16_t)zip->number_entry;
			noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}
	/* Total number of entries in the central dir */
	if (noerr)
	{
		if (zip->number_entry >= UINT16_MAX)
			noerr = fsWriteToStream(zip->stream, &u16_max, sizeof(u16_max)) == sizeof(u16_max);
		else
		{
			uint16_t tmp = (uint16_t)zip->number_entry;
			noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}
	/* Size of the central directory */
	if (noerr)
	{
		uint32_t tmp = (uint32_t)zip->cd_size;
		noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
	}
	/* Offset of start of central directory with respect to the starting disk number */
	if (noerr)
	{
		if (zip->cd_offset >= UINT32_MAX)
			noerr = fsWriteToStream(zip->stream, &u32_max, sizeof(u32_max)) == sizeof(u32_max);
		else
		{
			uint32_t tmp = (uint32_t)zip->cd_offset;
			noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
		}
	}

	/* Write global comment */
	if (zip->comment != NULL)
	{
		comment_size = (int32_t)strlen(zip->comment);
		if (comment_size > UINT16_MAX)
			comment_size = UINT16_MAX;
	}
	if (noerr)
	{
		uint16_t tmp = comment_size;
		noerr = fsWriteToStream(zip->stream, &tmp, sizeof(tmp)) == sizeof(tmp);
	}
	if (noerr)
	{
		if (fsWriteToStream(zip->stream, zip->comment, comment_size) != comment_size)
			noerr = false;
	}
	return noerr;
}

static bool mz_zip_recover_cd(void* handle)
{
	mz_zip*     zip = (mz_zip*)handle;
	mz_zip_file local_file_info;
	FileStream  local_file_info_stream = (FileStream){ 0 };
	FileStream* cd_mem_stream = NULL;
	uint64_t    number_entry = 0;
	ssize_t     descriptor_pos = 0;
	ssize_t     next_header_pos = 0;
	int64_t     disk_offset = 0;
	int64_t     disk_number = 0;
	int64_t     compressed_pos = 0;
	int64_t     compressed_end_pos = 0;
	int64_t     compressed_size = 0;
	int64_t     uncompressed_size = 0;
	uint8_t     descriptor_magic[4] = MZ_ZIP_MAGIC_DATADESCRIPTORU8;
	uint8_t     local_header_magic[4] = MZ_ZIP_MAGIC_LOCALHEADERU8;
	uint8_t     central_header_magic[4] = MZ_ZIP_MAGIC_CENTRALHEADERU8;
	uint32_t    crc32 = 0;
	int32_t     disk_number_with_cd = 0;
	bool        noerr = true;
	uint8_t     zip64 = 0;
	uint8_t     eof = 0;

	mz_zip_print("Zip - Recover - Start\n");

	mz_zip_get_cd_mem_stream(handle, &cd_mem_stream);

	/* Determine if we are on a split disk or not */
	fsGetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, 0);
	if (fsGetStreamSeekPosition(zip->stream) < 0)
	{
		fsSetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, -1);
		fsSeekStream(zip->stream, SBO_START_OF_FILE, 0);
	}
	else
		disk_number_with_cd = 1;

	// if (mz_stream_is_open(cd_mem_stream) != MZ_OK)
	if (cd_mem_stream->pIO == NULL)
		noerr = fsOpenStreamFromMemory(cd_mem_stream, 0, FM_READ_WRITE, true, cd_mem_stream);
	ASSERT(fsIsMemoryStream(cd_mem_stream));

	fsOpenStreamFromMemory(NULL, 0, FM_READ_WRITE, false, &local_file_info_stream);

	if (noerr)
	{
		noerr = fsFindStream(zip->stream, (const void*)local_header_magic, sizeof(local_header_magic), SSIZE_MAX, &next_header_pos);
	}

	while (noerr && !eof)
	{
		/* Get current offset and disk number for central dir record */
		disk_offset = fsGetStreamSeekPosition(zip->stream);
		fsGetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &disk_number);

		/* Read local headers */
		memset(&local_file_info, 0, sizeof(local_file_info));
		noerr = mz_zip_entry_read_header(zip->stream, 1, &local_file_info, &local_file_info_stream);
		if (!noerr)
			break;

		local_file_info.disk_offset = disk_offset;
		if (disk_number < 0)
			disk_number = 0;
		local_file_info.disk_number = (uint32_t)disk_number;

		compressed_pos = fsGetStreamSeekPosition(zip->stream);

		if ((noerr) && (local_file_info.compressed_size > 0))
		{
			fsSeekStream(zip->stream, SBO_CURRENT_POSITION, local_file_info.compressed_size);
		}

		for (;;)
		{
			/* Search for the next local header */
			noerr = fsFindStream(zip->stream, (const void*)local_header_magic, sizeof(local_header_magic), SSIZE_MAX, &next_header_pos);

			//if (err == MZ_EXIST_ERROR) {
			if (!noerr)
			{
				fsSeekStream(zip->stream, SBO_START_OF_FILE, compressed_pos);

				/* Search for central dir if no local header found */
				noerr =
					fsFindStream(zip->stream, (const void*)central_header_magic, sizeof(central_header_magic), SSIZE_MAX, &next_header_pos);

				//if (err == MZ_EXIST_ERROR) {
				if (!noerr)
				{
					/* Get end of stream if no central header found */
					fsSeekStream(zip->stream, SBO_END_OF_FILE, 0);
					next_header_pos = fsGetStreamSeekPosition(zip->stream);
				}

				eof = 1;
			}

			if (local_file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR || local_file_info.compressed_size == 0)
			{
				/* Search backwards for the descriptor, seeking too far back will be incorrect if compressed size is small */
				noerr = fsFindReverseStream(
					zip->stream, (const void*)descriptor_magic, sizeof(descriptor_magic), MZ_ZIP_SIZE_MAX_DATA_DESCRIPTOR, &descriptor_pos);
				if (noerr)
				{
					if (mz_zip_extrafield_contains(
							local_file_info.extrafield, local_file_info.extrafield_size, MZ_ZIP_EXTENSION_ZIP64, NULL))
						zip64 = 1;

					noerr = mz_zip_entry_read_descriptor(zip->stream, zip64, &crc32, &compressed_size, &uncompressed_size);

					if (noerr)
					{
						if (local_file_info.crc == 0)
							local_file_info.crc = crc32;
						if (local_file_info.compressed_size == 0)
							local_file_info.compressed_size = compressed_size;
						if (local_file_info.uncompressed_size == 0)
							local_file_info.uncompressed_size = uncompressed_size;
					}

					compressed_end_pos = descriptor_pos;
				}
				else if (eof)
				{
					compressed_end_pos = next_header_pos;
				}
				else if (local_file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR)
				{
					/* Wrong local file entry found, keep searching */
					next_header_pos += 1;
					fsSeekStream(zip->stream, SBO_START_OF_FILE, next_header_pos);
					continue;
				}
			}
			else
			{
				compressed_end_pos = next_header_pos;
			}

			break;
		}

		compressed_size = compressed_end_pos - compressed_pos;

		if (compressed_size > UINT32_MAX)
		{
			/* Update sizes if 4GB file is written with no ZIP64 support */
			if (local_file_info.uncompressed_size < UINT32_MAX)
			{
				local_file_info.compressed_size = compressed_size;
				local_file_info.uncompressed_size = 0;
			}
		}

		mz_zip_print(
			"Zip - Recover - Entry %s (csize %" PRId64 " usize %" PRId64 " flags 0x%" PRIx16 ")\n", local_file_info.filename,
			local_file_info.compressed_size, local_file_info.uncompressed_size, local_file_info.flag);

		/* Rewrite central dir with local headers and offsets */
		noerr = mz_zip_entry_write_header(cd_mem_stream, 0, &local_file_info);
		if (noerr)
			number_entry += 1;

		noerr = fsSeekStream(zip->stream, SBO_START_OF_FILE, next_header_pos);
	}

	fsCloseStream(&local_file_info_stream);

	mz_zip_print("Zip - Recover - Complete (cddisk %" PRId32 " entries %" PRId64 ")\n", disk_number_with_cd, number_entry);

	if (number_entry == 0)
		return noerr;

	/* Set new upper seek boundary for central dir mem stream */
	disk_offset = fsGetStreamSeekPosition(cd_mem_stream);
	cd_mem_stream->mSize = (ssize_t)disk_offset;

	/* Set new central directory info */
	mz_zip_set_cd_stream(handle, 0, cd_mem_stream);
	mz_zip_set_number_entry(handle, number_entry);
	mz_zip_set_disk_number_with_cd(handle, disk_number_with_cd);

	return true;
}

void* mz_zip_create(void** handle)
{
	mz_zip* zip = NULL;

	zip = (mz_zip*)MZ_ALLOC(sizeof(mz_zip));
	if (zip != NULL)
	{
		mz_zip_reset(zip);
	}
	if (handle != NULL)
		*handle = zip;

	return zip;
}

void mz_zip_delete(void** handle)
{
	if (handle == NULL)
		return;
	mz_zip* zip = (mz_zip*)*handle;
	if (zip != NULL)
	{
		MZ_FREE(zip);
	}
	*handle = NULL;
}

bool mz_zip_open(void* handle, FileStream* stream, FileMode mode)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;

	if (zip == NULL)
		return false;

	mz_zip_print("Zip - Open\n");
	zip->entry_open_mode = 0;
	zip->stream = stream;

	// if we write to file and it is empty(new file)
	bool create = (mode & (FM_WRITE | FM_APPEND) && fsGetStreamFileSize(stream) == 0);

	if (mode & (FM_WRITE | FM_APPEND))
	{
		fsOpenStreamFromMemory(NULL, 0, FM_READ_WRITE, false, &zip->cd_mem_stream);
		zip->cd_stream = &zip->cd_mem_stream;
	}
	else
	{
		zip->cd_stream = stream;
	}

	if ((mode & FM_READ) || (mode & FM_APPEND))
	{
		if (!create)
		{
			noerr = mz_zip_read_cd(zip);
			if (!noerr)
			{
				mz_zip_print("Zip - Error detected reading cd (%" PRId32 ")\n", err);
				if (zip->recover && mz_zip_recover_cd(zip))
					noerr = true;
			}
		}

		if (noerr && (mode & (FM_APPEND | FM_WRITE)))
		{
			if (zip->cd_size > 0)
			{
				/* Store central directory in memory */
				noerr = fsSeekStream(zip->stream, SBO_START_OF_FILE, zip->cd_offset);
				if (noerr)
					noerr = fsCopyStream(&zip->cd_mem_stream, zip->stream, (int32_t)zip->cd_size);
				if (noerr)
					noerr = fsSeekStream(zip->stream, SBO_START_OF_FILE, zip->cd_offset);
			}
			else
			{
				if (zip->cd_signature == mzZipMagicEndheader)
				{
					/* If tiny zip then overwrite end header */
					noerr = fsSeekStream(zip->stream, SBO_START_OF_FILE, zip->cd_offset);
				}
				else
				{
					/* If no central directory, append new zip to end of file */
					noerr = fsSeekStream(zip->stream, SBO_END_OF_FILE, 0);
				}
			}

			if (zip->disk_number_with_cd > 0)
			{
				/* Move to last disk to begin appending */
				fsSetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, zip->disk_number_with_cd - 1);
			}
		}
		else
		{
			zip->cd_start_pos = zip->cd_offset;
		}
	}

	if (!noerr)
	{
		mz_zip_close(zip);
		return false;
	}

	/* Memory streams used to store variable length file info data */
	fsOpenStreamFromMemory(NULL, 0, FM_READ_WRITE, false, &zip->file_info_stream);

	fsOpenStreamFromMemory(NULL, 0, FM_READ_WRITE, false, &zip->local_file_info_stream);

	zip->open_mode = mode;

	return noerr;
}

bool mz_zip_close(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;

	if (zip == NULL)
		return false;

	mz_zip_print("Zip - Close\n");

	if (mz_zip_entry_is_open(handle))
		noerr = mz_zip_entry_close(handle);

	if (noerr && (zip->open_mode & FM_WRITE))
	{
		fsSeekStream(zip->stream, SBO_CURRENT_POSITION, 0);
		noerr = mz_zip_write_cd(handle);
	}

	if (zip->cd_mem_stream.pIO != NULL)
	{
		fsCloseStream(&zip->cd_mem_stream);
	}

	if (zip->file_info_stream.pIO != NULL)
	{
		fsCloseStream(&zip->file_info_stream);
	}
	if (zip->local_file_info_stream.pIO != NULL)
	{
		fsCloseStream(&zip->local_file_info_stream);
	}

	if (zip->comment)
	{
		MZ_FREE(zip->comment);
	}

	mz_zip_reset(zip);

	return noerr;
}

bool mz_zip_get_comment(void* handle, const char** comment)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || comment == NULL)
		return false;
	if (zip->comment == NULL)
		return false;
	*comment = zip->comment;
	return true;
}

bool mz_zip_set_comment(void* handle, const char* comment)
{
	mz_zip* zip = (mz_zip*)handle;
	int32_t comment_size = 0;
	if (zip == NULL || comment == NULL)
		return false;
	if (zip->comment != NULL)
		MZ_FREE(zip->comment);
	comment_size = (int32_t)strlen(comment);
	if (comment_size > UINT16_MAX)
		return false;
	zip->comment = (char*)MZ_ALLOC(comment_size + 1);
	if (zip->comment == NULL)
		return false;
	memset(zip->comment, 0, comment_size + 1);
	strncpy(zip->comment, comment, comment_size);
	return true;
}

bool mz_zip_get_version_madeby(void* handle, uint16_t* version_madeby)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || version_madeby == NULL)
		return false;
	*version_madeby = zip->version_madeby;
	return true;
}

bool mz_zip_set_version_madeby(void* handle, uint16_t version_madeby)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL)
		return false;
	zip->version_madeby = version_madeby;
	return true;
}

bool mz_zip_set_recover(void* handle, uint8_t recover)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL)
		return false;
	zip->recover = recover;
	return true;
}

bool mz_zip_set_data_descriptor(void* handle, uint8_t data_descriptor)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL)
		return false;
	zip->data_descriptor = data_descriptor;
	return true;
}

bool mz_zip_get_stream(void* handle, FileStream** stream)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || stream == NULL)
		return false;
	*stream = zip->stream;
	if (*stream == NULL)
		return false;
	return true;
}

bool mz_zip_set_cd_stream(void* handle, int64_t cd_start_pos, FileStream* cd_stream)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || cd_stream == NULL)
		return false;
	zip->cd_offset = 0;
	zip->cd_stream = cd_stream;
	zip->cd_start_pos = cd_start_pos;
	return true;
}

bool mz_zip_get_cd_mem_stream(void* handle, FileStream** cd_mem_stream)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || cd_mem_stream == NULL)
		return false;
	*cd_mem_stream = &zip->cd_mem_stream;
	if (*cd_mem_stream == NULL)
		return false;
	return true;
}

bool mz_zip_set_number_entry(void* handle, uint64_t number_entry)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL)
		return false;
	zip->number_entry = number_entry;
	return true;
}

bool mz_zip_get_number_entry(void* handle, uint64_t* number_entry)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || number_entry == NULL)
		return false;
	*number_entry = zip->number_entry;
	return true;
}

bool mz_zip_set_disk_number_with_cd(void* handle, uint32_t disk_number_with_cd)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL)
		return false;
	zip->disk_number_with_cd = disk_number_with_cd;
	return true;
}

bool mz_zip_get_disk_number_with_cd(void* handle, uint32_t* disk_number_with_cd)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || disk_number_with_cd == NULL)
		return false;
	*disk_number_with_cd = zip->disk_number_with_cd;
	return true;
}

static bool mz_zip_entry_close_int(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip->crypt_stream.pIO != NULL)
		fsCloseStream(&zip->crypt_stream);
	memset(&zip->crypt_stream, 0, sizeof(zip->crypt_stream));
	if (zip->compress_stream.pIO != NULL)
		fsCloseStream(&zip->compress_stream);
	memset(&zip->compress_stream, 0, sizeof(zip->compress_stream));

	zip->entry_opened = 0;

	return true;
}

static bool mz_zip_entry_open_int(void* handle, uint8_t raw, int16_t compress_level, const char* password)
{
	mz_zip* zip = (mz_zip*)handle;
	int64_t max_total_in = 0;
	int64_t header_size = 0;
	int64_t footer_size = 0;
	bool    noerr = true;
	uint8_t use_crypt = 0;

	if (zip == NULL)
		return false;

	switch (zip->file_info.compression_method)
	{
		case MZ_COMPRESS_METHOD_STORE:
		case MZ_COMPRESS_METHOD_DEFLATE:
#ifdef HAVE_BZIP2
		case MZ_COMPRESS_METHOD_BZIP2:
#endif
#ifdef HAVE_LZMA
		case MZ_COMPRESS_METHOD_LZMA:
#endif
#if defined(HAVE_LZMA) || defined(HAVE_LIBCOMP)
		case MZ_COMPRESS_METHOD_XZ:
#endif
#ifdef HAVE_ZSTD
		case MZ_COMPRESS_METHOD_ZSTD:
#endif
			noerr = true;
			break;
		default: return false;
	}

#ifndef HAVE_WZAES
	if (zip->file_info.aes_version)
		return false;
#endif

	zip->entry_raw = raw;

	if ((zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED) && (password != NULL))
	{
		if (zip->entry_open_mode & FM_WRITE)
		{
			/* Encrypt only when we are not trying to write raw and password is supplied. */
			if (!zip->entry_raw)
				use_crypt = 1;
		}
		else if (zip->entry_open_mode & FM_READ)
		{
			/* Decrypt only when password is supplied. Don't error when password */
			/* is not supplied as we may want to read the raw encrypted data. */
			use_crypt = 1;
		}
	}

	if ((noerr) && (use_crypt))
	{
#ifdef HAVE_WZAES
		if (zip->file_info.aes_version)
		{
			noerr =
				mz_stream_wzaes_open(password, zip->file_info.aes_encryption_mode, zip->entry_open_mode, zip->stream, &zip->crypt_stream);
		}
		else
#endif
		{
#ifdef HAVE_PKCRYPT
			uint8_t verify1 = (uint8_t)((zip->file_info.pk_verify >> 8) & 0xff);
			uint8_t verify2 = (uint8_t)((zip->file_info.pk_verify) & 0xff);

			mz_stream_pkcrypt_create(&zip->crypt_stream);
			mz_stream_pkcrypt_set_password(zip->crypt_stream, password);
			mz_stream_pkcrypt_set_verify(zip->crypt_stream, verify1, verify2);
#endif
		}
	}

	if (noerr)
	{
		if (zip->crypt_stream.pIO == NULL)
			noerr = mz_stream_raw_open(zip->stream, &zip->crypt_stream);
	}

	if (noerr)
	{
		if (zip->entry_raw || zip->file_info.compression_method == MZ_COMPRESS_METHOD_STORE)
			noerr = mz_stream_raw_open(&zip->crypt_stream, &zip->compress_stream);
#ifdef HAVE_ZLIB
		else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_DEFLATE)
			noerr = mzStreamZlibOpen(&zip->crypt_stream, zip->entry_open_mode, &zip->compress_stream);
#endif
#ifdef HAVE_BZIP2
		else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_BZIP2)
			mz_stream_bzip_create(&zip->compress_stream);
#endif
#ifdef HAVE_LIBCOMP
		else if (
			zip->file_info.compression_method == MZ_COMPRESS_METHOD_DEFLATE || zip->file_info.compression_method == MZ_COMPRESS_METHOD_XZ)
		{
			mz_stream_libcomp_create(&zip->compress_stream);
			fsSetStreamPropInt64(zip->compress_stream, MZ_STREAM_PROP_COMPRESS_METHOD, zip->file_info.compression_method);
		}
#endif
#ifdef HAVE_LZMA
		else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_LZMA || zip->file_info.compression_method == MZ_COMPRESS_METHOD_XZ)
		{
			mz_stream_lzma_create(&zip->compress_stream);
			fsSetStreamPropInt64(zip->compress_stream, MZ_STREAM_PROP_COMPRESS_METHOD, zip->file_info.compression_method);
		}
#endif
#ifdef HAVE_ZSTD
		else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_ZSTD)
			mz_stream_zstd_create(&zip->compress_stream);
#endif
		else
			noerr = false;
	}

	if (noerr)
	{
		if (zip->entry_open_mode & FM_WRITE)
		{
			fsSetStreamPropInt64(&zip->compress_stream, MZ_STREAM_PROP_COMPRESS_LEVEL, compress_level);
		}
		else
		{
			int32_t set_end_of_stream = 0;

#ifndef HAVE_LIBCOMP
			if (zip->entry_raw || zip->file_info.compression_method == MZ_COMPRESS_METHOD_STORE ||
				zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED)
#endif
			{
				max_total_in = zip->file_info.compressed_size;
				fsSetStreamPropInt64(&zip->crypt_stream, MZ_STREAM_PROP_TOTAL_IN_MAX, max_total_in);

				if (fsGetStreamPropInt64(&zip->crypt_stream, MZ_STREAM_PROP_HEADER_SIZE, &header_size))
					max_total_in -= header_size;
				if (fsGetStreamPropInt64(&zip->crypt_stream, MZ_STREAM_PROP_FOOTER_SIZE, &footer_size))
					max_total_in -= footer_size;

				fsSetStreamPropInt64(&zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN_MAX, max_total_in);
			}

			switch (zip->file_info.compression_method)
			{
				case MZ_COMPRESS_METHOD_LZMA:
				case MZ_COMPRESS_METHOD_XZ: set_end_of_stream = (zip->file_info.flag & MZ_ZIP_FLAG_LZMA_EOS_MARKER); break;
				case MZ_COMPRESS_METHOD_ZSTD: set_end_of_stream = 1; break;
			}

			if (set_end_of_stream)
			{
				fsSetStreamPropInt64(&zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN_MAX, zip->file_info.compressed_size);
				fsSetStreamPropInt64(&zip->compress_stream, MZ_STREAM_PROP_TOTAL_OUT_MAX, zip->file_info.uncompressed_size);
			}
		}

		//mz_stream_set_base(zip->compress_stream, zip->crypt_stream);

		//err = mz_stream_open(zip->compress_stream, NULL, zip->open_mode);
	}

	if (noerr)
	{
		zip->entry_opened = 1;
		zip->entry_crc32 = 0;
	}
	else
	{
		mz_zip_entry_close_int(handle);
	}

	return noerr;
}

bool mz_zip_entry_is_open(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL)
		return false;
	if (zip->entry_opened == 0)
		return false;
	return true;
}

bool mz_zip_entry_read_open(void* handle, uint8_t raw, const char* password)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;
	bool    noerr_shift = true;

#if defined(MZ_ZIP_NO_ENCRYPTION)
	if (password != NULL)
		return MZ_SUPPORT_ERROR;
#endif
	if (zip == NULL)
		return false;
	if ((zip->open_mode & FM_READ) == 0)
		return false;
	if (zip->entry_scanned == 0)
		return false;

	if (zip->entry_open_mode != FM_READ)
		fsSeekStream(zip->stream, SBO_CURRENT_POSITION, 0);
	zip->entry_open_mode = FM_READ;

	mz_zip_print("Zip - Entry - Read open (raw %" PRId32 ")\n", raw);

	noerr = mz_zip_entry_seek_local_header(handle);
	if (noerr)
		noerr = mz_zip_entry_read_header(zip->stream, 1, &zip->local_file_info, &zip->local_file_info_stream);

	if (!noerr && zip->disk_offset_shift > 0)
	{
		/* Perhaps we didn't compensated correctly for incorrect cd offset */
		noerr_shift = fsSeekStream(zip->stream, SBO_START_OF_FILE, zip->file_info.disk_offset);
		if (noerr_shift)
			noerr_shift = mz_zip_entry_read_header(zip->stream, 1, &zip->local_file_info, &zip->local_file_info_stream);
		if (noerr_shift)
		{
			zip->disk_offset_shift = 0;
			noerr = noerr_shift;
		}
	}

#ifdef MZ_ZIP_NO_DECOMPRESSION
	if (!raw && zip->file_info.compression_method != MZ_COMPRESS_METHOD_STORE)
		err = MZ_SUPPORT_ERROR;
#endif
	if (noerr)
		noerr = mz_zip_entry_open_int(handle, raw, 0, password);

	return noerr;
}

bool mz_zip_entry_write_open(void* handle, const mz_zip_file* file_info, int16_t compress_level, uint8_t raw, const char* password)
{
	mz_zip* zip = (mz_zip*)handle;
	int64_t filename_pos = -1;
	int64_t extrafield_pos = 0;
	int64_t comment_pos = 0;
	int64_t linkname_pos = 0;
	int64_t disk_number = 0;
	uint8_t is_dir = 0;
	bool    noerr = true;

	if ((zip->open_mode & FM_WRITE) == 0)
		return false;

	if (zip->entry_open_mode != FM_WRITE)
		fsSeekStream(zip->stream, SBO_CURRENT_POSITION, 0);
	zip->entry_open_mode = FM_WRITE;

#if defined(MZ_ZIP_NO_ENCRYPTION)
	if (password != NULL)
		return MZ_SUPPORT_ERROR;
#endif
	if (zip == NULL || file_info == NULL || file_info->filename == NULL)
		return false;

	if (mz_zip_entry_is_open(handle))
	{
		noerr = mz_zip_entry_close(handle);
		if (!noerr)
			return false;
	}

	memcpy(&zip->file_info, file_info, sizeof(mz_zip_file));

	mz_zip_print("Zip - Entry - Write open - %s (level %" PRId16 " raw %" PRId8 ")\n", zip->file_info.filename, compress_level, raw);

	fsSeekStream(&zip->file_info_stream, SBO_START_OF_FILE, 0);
	fsWriteToStream(&zip->file_info_stream, file_info, sizeof(mz_zip_file));

	/* Copy filename, extrafield, and comment internally */
	filename_pos = fsGetStreamSeekPosition(&zip->file_info_stream);
	if (file_info->filename != NULL)
		fsWriteToStream(&zip->file_info_stream, file_info->filename, (int32_t)strlen(file_info->filename));
	fsWriteToStream(&zip->file_info_stream, &u8_zero, sizeof(u8_zero));

	extrafield_pos = fsGetStreamSeekPosition(&zip->file_info_stream);
	if (file_info->extrafield != NULL)
		fsWriteToStream(&zip->file_info_stream, file_info->extrafield, file_info->extrafield_size);
	fsWriteToStream(&zip->file_info_stream, &u8_zero, sizeof(u8_zero));

	comment_pos = fsGetStreamSeekPosition(&zip->file_info_stream);
	if (file_info->comment != NULL)
		fsWriteToStream(&zip->file_info_stream, file_info->comment, file_info->comment_size);
	fsWriteToStream(&zip->file_info_stream, &u8_zero, sizeof(u8_zero));

	linkname_pos = fsGetStreamSeekPosition(&zip->file_info_stream);
	if (file_info->linkname != NULL)
		fsWriteToStream(&zip->file_info_stream, file_info->linkname, (int32_t)strlen(file_info->linkname));
	fsWriteToStream(&zip->file_info_stream, &u8_zero, sizeof(u8_zero));

	ASSERT(fsIsMemoryStream(&zip->file_info_stream));
	fsGetMemoryStreamBufferAt(&zip->file_info_stream, filename_pos, (const void**)&zip->file_info.filename);
	fsGetMemoryStreamBufferAt(&zip->file_info_stream, extrafield_pos, (const void**)&zip->file_info.extrafield);
	fsGetMemoryStreamBufferAt(&zip->file_info_stream, comment_pos, (const void**)&zip->file_info.comment);
	fsGetMemoryStreamBufferAt(&zip->file_info_stream, linkname_pos, (const void**)&zip->file_info.linkname);

	if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_DEFLATE)
	{
		if ((compress_level == 8) || (compress_level == 9))
			zip->file_info.flag |= MZ_ZIP_FLAG_DEFLATE_MAX;
		if (compress_level == 2)
			zip->file_info.flag |= MZ_ZIP_FLAG_DEFLATE_FAST;
		if (compress_level == 1)
			zip->file_info.flag |= MZ_ZIP_FLAG_DEFLATE_SUPER_FAST;
	}
#if defined(HAVE_LZMA) || defined(HAVE_LIBCOMP)
	else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_LZMA || zip->file_info.compression_method == MZ_COMPRESS_METHOD_XZ)
		zip->file_info.flag |= MZ_ZIP_FLAG_LZMA_EOS_MARKER;
#endif

	if (mz_zip_attrib_is_dir(zip->file_info.external_fa, zip->file_info.version_madeby))
		is_dir = 1;

	if (!is_dir)
	{
		if (zip->data_descriptor)
			zip->file_info.flag |= MZ_ZIP_FLAG_DATA_DESCRIPTOR;
		if (password != NULL)
			zip->file_info.flag |= MZ_ZIP_FLAG_ENCRYPTED;
	}

	fsGetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &disk_number);
	zip->file_info.disk_number = (uint32_t)disk_number;
	zip->file_info.disk_offset = fsGetStreamSeekPosition(zip->stream);

	if (zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED)
	{
#ifdef HAVE_PKCRYPT
		/* Pre-calculated CRC value is required for PKWARE traditional encryption */
		uint32_t dos_date = mz_zip_time_t_to_dos_date(zip->file_info.modified_date);
		zip->file_info.pk_verify = mz_zip_get_pk_verify(dos_date, zip->file_info.crc, zip->file_info.flag);
#endif
#ifdef HAVE_WZAES
		if (zip->file_info.aes_version && zip->file_info.aes_encryption_mode == 0)
			zip->file_info.aes_encryption_mode = MZ_AES_ENCRYPTION_MODE_256;
#endif
	}

	zip->file_info.crc = 0;
	zip->file_info.compressed_size = 0;

	if ((compress_level == 0) || (is_dir))
		zip->file_info.compression_method = MZ_COMPRESS_METHOD_STORE;

#ifdef MZ_ZIP_NO_COMPRESSION
	if (zip->file_info.compression_method != MZ_COMPRESS_METHOD_STORE)
		err = MZ_SUPPORT_ERROR;
#endif
	if (noerr)
		noerr = mz_zip_entry_write_header(zip->stream, 1, &zip->file_info);
	if (noerr)
		noerr = mz_zip_entry_open_int(handle, raw, compress_level, password);

	return noerr;
}

size_t mz_zip_entry_read(void* handle, void* buf, size_t len)
{
	mz_zip* zip = (mz_zip*)handle;
	size_t  read = 0;

	if (zip == NULL || !mz_zip_entry_is_open(handle))
		return false;
	if (UINT_MAX == UINT16_MAX && len > UINT16_MAX) /* zlib limitation */
		return false;
	if (len == 0)
		return false;

	if (zip->file_info.compressed_size == 0)
		return 0;

	/* Read entire entry even if uncompressed_size = 0, otherwise */
	/* aes encryption validation will fail if compressed_size > 0 */
	read = fsReadFromStream(&zip->compress_stream, buf, len);
	if (read > 0)
		zip->entry_crc32 = mz_crypt_crc32_update(zip->entry_crc32, (const uint8_t*)buf, read);

	mz_zip_print("Zip - Entry - Read - %" PRId32 " (max %" PRId32 ")\n", read, len);

	return read;
}

size_t mz_zip_entry_write(void* handle, const void* buf, size_t len)
{
	mz_zip* zip = (mz_zip*)handle;
	size_t  written = 0;

	ASSERT(zip != NULL && mz_zip_entry_is_open(handle));

	written = fsWriteToStream(&zip->compress_stream, buf, len);
	if (written > 0)
		zip->entry_crc32 = mz_crypt_crc32_update(zip->entry_crc32, (const uint8_t*)buf, written);

	mz_zip_print("Zip - Entry - Write - %" PRId32 " (max %" PRId32 ")\n", written, len);

	return written;
}

bool mz_zip_entry_read_close(void* handle, uint32_t* crc32, int64_t* compressed_size, int64_t* uncompressed_size)
{
	mz_zip* zip = (mz_zip*)handle;
	int64_t total_in = 0;
	bool    noerr = true;
	uint8_t zip64 = 0;

	if (zip == NULL || !mz_zip_entry_is_open(handle))
		return false;

	fsFlushStream(&zip->compress_stream);

	fsGetStreamPropInt64(&zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN, &total_in);

	fsCloseStream(&zip->compress_stream);

	mz_zip_print("Zip - Entry - Read Close\n");

	if (crc32 != NULL)
		*crc32 = zip->file_info.crc;
	if (compressed_size != NULL)
		*compressed_size = zip->file_info.compressed_size;
	if (uncompressed_size != NULL)
		*uncompressed_size = zip->file_info.uncompressed_size;

	if ((zip->file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR) && ((zip->file_info.flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO) == 0) &&
		(crc32 != NULL || compressed_size != NULL || uncompressed_size != NULL))
	{
		/* Check to see if data descriptor is zip64 bit format or not */
		if (mz_zip_extrafield_contains(zip->local_file_info.extrafield, zip->local_file_info.extrafield_size, MZ_ZIP_EXTENSION_ZIP64, NULL))
			zip64 = 1;

		noerr = mz_zip_entry_seek_local_header(handle);

		/* Seek to end of compressed stream since we might have over-read during compression */
		if (noerr)
			noerr = fsSeekStream(
				zip->stream, SBO_START_OF_FILE,
				MZ_ZIP_SIZE_LD_ITEM + (int64_t)zip->local_file_info.filename_size + (int64_t)zip->local_file_info.extrafield_size +
					total_in);

		/* Read data descriptor */
		if (noerr)
			noerr = mz_zip_entry_read_descriptor(zip->stream, zip64, crc32, compressed_size, uncompressed_size);
	}

	/* If entire entry was not read verification will fail */
	if (noerr && (total_in > 0) && (!zip->entry_raw))
	{
#ifdef HAVE_WZAES
		/* AES zip version AE-1 will expect a valid crc as well */
		if (zip->file_info.aes_version <= 0x0001)
#endif
		{
			if (zip->entry_crc32 != zip->file_info.crc)
			{
				mz_zip_print(
					"Zip - Entry - Crc failed (actual 0x%08" PRIx32 " expected 0x%08" PRIx32 ")\n", zip->entry_crc32, zip->file_info.crc);

				noerr = false;
			}
		}
	}

	mz_zip_entry_close_int(handle);

	return noerr;
}

bool mz_zip_entry_write_close(void* handle, uint32_t crc32, int64_t compressed_size, int64_t uncompressed_size)
{
	mz_zip* zip = (mz_zip*)handle;
	int64_t end_disk_number = 0;
	bool    noerr = true;
	uint8_t zip64 = 0;

	if (zip == NULL || !mz_zip_entry_is_open(handle))
		return false;

	fsFlushStream(&zip->compress_stream);

	if (!zip->entry_raw)
		crc32 = zip->entry_crc32;

	mz_zip_print(
		"Zip - Entry - Write Close (crc 0x%08" PRIx32 " cs %" PRId64 " ucs %" PRId64 ")\n", crc32, compressed_size, uncompressed_size);

	/* If sizes are not set, then read them from the compression stream */
	if (compressed_size < 0)
		fsGetStreamPropInt64(&zip->compress_stream, MZ_STREAM_PROP_TOTAL_OUT, &compressed_size);
	if (uncompressed_size < 0)
		fsGetStreamPropInt64(&zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN, &uncompressed_size);

	fsCloseStream(&zip->compress_stream);

	if (zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED)
	{
		zip->crypt_stream.pBase = zip->stream;
		noerr = fsFlushStream(&zip->crypt_stream);

		fsGetStreamPropInt64(&zip->crypt_stream, MZ_STREAM_PROP_TOTAL_OUT, &compressed_size);
		noerr |= fsCloseStream(&zip->crypt_stream);
	}

	mz_zip_entry_needs_zip64(&zip->file_info, 1, &zip64);

	if (noerr && (zip->file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR))
	{
		/* Determine if we need to write data descriptor in zip64 format,
           if local extrafield was saved with zip64 extrafield */

		if (zip->file_info.flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO)
			noerr = mz_zip_entry_write_descriptor(zip->stream, zip64, 0, compressed_size, 0);
		else
			noerr = mz_zip_entry_write_descriptor(zip->stream, zip64, crc32, compressed_size, uncompressed_size);
	}

	/* Write file info to central directory */

	mz_zip_print(
		"Zip - Entry - Write cd (ucs %" PRId64 " cs %" PRId64 " crc 0x%08" PRIx32 ")\n", uncompressed_size, compressed_size, crc32);

	zip->file_info.crc = crc32;
	zip->file_info.compressed_size = compressed_size;
	zip->file_info.uncompressed_size = uncompressed_size;

	if (noerr)
		noerr = mz_zip_entry_write_header(&zip->cd_mem_stream, 0, &zip->file_info);

	/* Update local header with crc32 and sizes */
	if ((noerr) && ((zip->file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR) == 0) && ((zip->file_info.flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO) == 0))
	{
		/* Save the disk number and position we are to seek back after updating local header */
		int64_t end_pos = fsGetStreamSeekPosition(zip->stream);
		fsGetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &end_disk_number);

		noerr = mz_zip_entry_seek_local_header(handle);

		if (noerr)
		{
			/* Seek to crc32 and sizes offset in local header */
			noerr = fsSeekStream(zip->stream, SBO_CURRENT_POSITION, MZ_ZIP_OFFSET_CRC_SIZES);
		}

		if (noerr)
			noerr = mz_zip_entry_write_crc_sizes(zip->stream, zip64, 0, &zip->file_info);

		/* Seek to and update zip64 extension sizes */
		if ((noerr) && (zip64))
		{
			int64_t filename_size = zip->file_info.filename_size;

			if (filename_size == 0)
				filename_size = strlen(zip->file_info.filename);

			/* Since we write zip64 extension first we know its offset */
			noerr = fsSeekStream(zip->stream, SBO_CURRENT_POSITION, 2 + 2 + filename_size + 4);

			if (noerr)
				noerr = fsWriteToStream(zip->stream, &zip->file_info.uncompressed_size, sizeof(zip->file_info.uncompressed_size)) ==
						sizeof(zip->file_info.uncompressed_size);
			if (noerr)
				noerr = fsWriteToStream(zip->stream, &zip->file_info.compressed_size, sizeof(zip->file_info.compressed_size)) ==
						sizeof(zip->file_info.compressed_size);
		}

		fsSetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, end_disk_number);
		fsSeekStream(zip->stream, SBO_START_OF_FILE, end_pos);
	}

	zip->number_entry += 1;

	mz_zip_entry_close_int(handle);

	return noerr;
}

bool mz_zip_entry_seek_local_header(void* handle)
{
	mz_zip*  zip = (mz_zip*)handle;
	int64_t  disk_size = 0;
	uint32_t disk_number = zip->file_info.disk_number;

	if (disk_number == zip->disk_number_with_cd)
	{
		fsGetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_SIZE, &disk_size);
		if ((disk_size == 0) || ((zip->entry_open_mode & FM_WRITE) == 0))
			disk_number = (uint32_t)-1;
	}

	fsSetStreamPropInt64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, disk_number);

	mz_zip_print("Zip - Entry - Seek local (disk %" PRId32 " offset %" PRId64 ")\n", disk_number, zip->file_info.disk_offset);

	/* Guard against seek overflows */
	if ((zip->disk_offset_shift > 0) && (zip->file_info.disk_offset > (INT64_MAX - zip->disk_offset_shift)))
		return false;

	return fsSeekStream(zip->stream, SBO_START_OF_FILE, zip->file_info.disk_offset + zip->disk_offset_shift);
}

bool mz_zip_entry_close(void* handle) { return mz_zip_entry_close_raw(handle, UINT64_MAX, 0); }

bool mz_zip_entry_close_raw(void* handle, int64_t uncompressed_size, uint32_t crc32)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;

	if (zip == NULL || !mz_zip_entry_is_open(handle))
		return false;

	if (zip->entry_open_mode & FM_WRITE)
		noerr = mz_zip_entry_write_close(handle, crc32, UINT64_MAX, uncompressed_size);
	else
		noerr = mz_zip_entry_read_close(handle, NULL, NULL, NULL);

	return noerr;
}

bool mz_zip_entry_is_dir(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;
	int32_t filename_length = 0;

	if (zip == NULL)
		return false;
	if (zip->entry_scanned == 0)
		return false;
	if (mz_zip_attrib_is_dir(zip->file_info.external_fa, zip->file_info.version_madeby))
		return true;

	filename_length = (int32_t)strlen(zip->file_info.filename);
	if (filename_length > 0)
	{
		if ((zip->file_info.filename[filename_length - 1] == '/') || (zip->file_info.filename[filename_length - 1] == '\\'))
			return true;
	}
	return false;
}

bool mz_zip_entry_is_symlink(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip == NULL)
		return false;
	if (zip->entry_scanned == 0)
		return false;
	if (!mz_zip_attrib_is_symlink(zip->file_info.external_fa, zip->file_info.version_madeby))
		return false;
	if (zip->file_info.linkname == NULL || *zip->file_info.linkname == 0)
		return false;

	return true;
}

bool mz_zip_entry_get_info(void* handle, mz_zip_file** file_info)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip == NULL)
		return false;

	if ((zip->entry_open_mode & FM_WRITE) == 0)
	{
		if (!zip->entry_scanned)
			return false;
	}

	*file_info = &zip->file_info;
	return true;
}

bool mz_zip_entry_get_local_info(void* handle, mz_zip_file** local_file_info)
{
	mz_zip* zip = (mz_zip*)handle;
	if (zip == NULL || !mz_zip_entry_is_open(handle))
		return false;
	*local_file_info = &zip->local_file_info;
	return true;
}

bool mz_zip_entry_set_extrafield(void* handle, const uint8_t* extrafield, uint16_t extrafield_size)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip == NULL || !mz_zip_entry_is_open(handle))
		return false;

	zip->file_info.extrafield = extrafield;
	zip->file_info.extrafield_size = extrafield_size;
	return true;
}

static bool mz_zip_goto_next_entry_int(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;

	if (zip == NULL)
		return false;

	zip->entry_scanned = 0;

	fsSetStreamPropInt64(zip->cd_stream, MZ_STREAM_PROP_DISK_NUMBER, -1);

	noerr = fsSeekStream(zip->cd_stream, SBO_START_OF_FILE, zip->cd_current_pos);
	if (noerr)
		noerr = mz_zip_entry_read_header(zip->cd_stream, 0, &zip->file_info, &zip->file_info_stream);
	if (noerr)
		zip->entry_scanned = 1;
	return noerr;
}

uint64_t mz_zip_get_entry(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip == NULL)
		return false;

	return zip->cd_current_entry;
}

bool mz_zip_goto_entry(void* handle, uint64_t id)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip == NULL)
		return false;

	uint64_t entryCount;
	bool noerr = mz_zip_get_number_entry(handle, &entryCount);
	ASSERT(noerr);

	if (id >= entryCount)
		return false;

	if ((noerr && zip->cd_current_entry > id) || zip->cd_current_entry == 0)
		noerr = mz_zip_goto_first_entry(handle);

	while (noerr && zip->cd_current_entry < id) 
	{
		noerr = mz_zip_goto_next_entry(zip);
	}
	
	ASSERT(!noerr || zip->cd_current_entry == id);

	return noerr;
}

bool mz_zip_goto_first_entry(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip == NULL)
		return false;

	zip->cd_current_pos = zip->cd_start_pos;
	zip->cd_current_entry = 0;

	return mz_zip_goto_next_entry_int(handle);
}

bool mz_zip_goto_next_entry(void* handle)
{
	mz_zip* zip = (mz_zip*)handle;

	if (zip == NULL)
		return false;

	zip->cd_current_pos +=
		(int64_t)MZ_ZIP_SIZE_CD_ITEM + zip->file_info.filename_size + zip->file_info.extrafield_size + zip->file_info.comment_size;
	zip->cd_current_entry += 1;

	return mz_zip_goto_next_entry_int(handle);
}

bool mz_zip_locate_entry(void* handle, const char* filename, uint8_t ignore_case)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;
	int32_t result = 0;

	if (zip == NULL || filename == NULL)
		return false;

	/* If we are already on the current entry, no need to search */
	if ((zip->entry_scanned) && (zip->file_info.filename != NULL))
	{
		result = mz_zip_path_compare(zip->file_info.filename, filename, ignore_case);
		if (result == 0)
			return true;
	}

	/* Search all entries starting at the first */
	noerr = mz_zip_goto_first_entry(handle);
	while (noerr)
	{
		result = mz_zip_path_compare(zip->file_info.filename, filename, ignore_case);
		if (result == 0)
			return true;

		noerr = mz_zip_goto_next_entry(handle);
	}

	return noerr;
}

bool mz_zip_locate_first_entry(void* handle, void* userdata, mz_zip_locate_entry_cb cb)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;
	int32_t result = 0;

	/* Search first entry looking for match */
	noerr = mz_zip_goto_first_entry(handle);
	if (!noerr)
		return false;

	result = cb(handle, userdata, &zip->file_info);
	if (result == 0)
		return false;

	return mz_zip_locate_next_entry(handle, userdata, cb);
}

bool mz_zip_locate_next_entry(void* handle, void* userdata, mz_zip_locate_entry_cb cb)
{
	mz_zip* zip = (mz_zip*)handle;
	bool    noerr = true;
	int32_t result = 0;

	/* Search next entries looking for match */
	noerr = mz_zip_goto_next_entry(handle);
	while (noerr)
	{
		result = cb(handle, userdata, &zip->file_info);
		if (result == 0)
			return true;

		noerr = mz_zip_goto_next_entry(handle);
	}

	return noerr;
}

/***************************************************************************/

bool mz_zip_attrib_is_dir(uint32_t attrib, int32_t version_madeby)
{
	uint32_t posix_attrib = 0;
	uint8_t  system = MZ_HOST_SYSTEM(version_madeby);
	bool     noerr = true;

	noerr = mz_zip_attrib_convert(system, attrib, MZ_HOST_SYSTEM_UNIX, &posix_attrib);
	if (noerr)
	{
		if ((posix_attrib & 0170000) == 0040000) /* S_ISDIR */
			return true;
	}

	return false;
}

bool mz_zip_attrib_is_symlink(uint32_t attrib, int32_t version_madeby)
{
	uint32_t posix_attrib = 0;
	uint8_t  system = MZ_HOST_SYSTEM(version_madeby);
	bool     noerr = true;

	noerr = mz_zip_attrib_convert(system, attrib, MZ_HOST_SYSTEM_UNIX, &posix_attrib);
	if (noerr)
	{
		if ((posix_attrib & 0170000) == 0120000) /* S_ISLNK */
			return true;
	}

	return false;
}

bool mz_zip_attrib_convert(uint8_t src_sys, uint32_t src_attrib, uint8_t target_sys, uint32_t* target_attrib)
{
	if (target_attrib == NULL)
		return false;

	*target_attrib = 0;

	if ((src_sys == MZ_HOST_SYSTEM_MSDOS) || (src_sys == MZ_HOST_SYSTEM_WINDOWS_NTFS))
	{
		if ((target_sys == MZ_HOST_SYSTEM_MSDOS) || (target_sys == MZ_HOST_SYSTEM_WINDOWS_NTFS))
		{
			*target_attrib = src_attrib;
			return true;
		}
		if ((target_sys == MZ_HOST_SYSTEM_UNIX) || (target_sys == MZ_HOST_SYSTEM_OSX_DARWIN) || (target_sys == MZ_HOST_SYSTEM_RISCOS))
			return mz_zip_attrib_win32_to_posix(src_attrib, target_attrib);
	}
	else if ((src_sys == MZ_HOST_SYSTEM_UNIX) || (src_sys == MZ_HOST_SYSTEM_OSX_DARWIN) || (src_sys == MZ_HOST_SYSTEM_RISCOS))
	{
		if ((target_sys == MZ_HOST_SYSTEM_UNIX) || (target_sys == MZ_HOST_SYSTEM_OSX_DARWIN) || (target_sys == MZ_HOST_SYSTEM_RISCOS))
		{
			/* If high bytes are set, it contains unix specific attributes */
			if ((src_attrib >> 16) != 0)
				src_attrib >>= 16;

			*target_attrib = src_attrib;
			return true;
		}
		if ((target_sys == MZ_HOST_SYSTEM_MSDOS) || (target_sys == MZ_HOST_SYSTEM_WINDOWS_NTFS))
			return mz_zip_attrib_posix_to_win32(src_attrib, target_attrib);
	}

	return false;
}

bool mz_zip_attrib_posix_to_win32(uint32_t posix_attrib, uint32_t* win32_attrib)
{
	if (win32_attrib == NULL)
		return false;

	*win32_attrib = 0;

	/* S_IWUSR | S_IWGRP | S_IWOTH | S_IXUSR | S_IXGRP | S_IXOTH */
	if ((posix_attrib & 0000333) == 0 && (posix_attrib & 0000444) != 0)
		*win32_attrib |= 0x01; /* FILE_ATTRIBUTE_READONLY */
	/* S_IFLNK */
	if ((posix_attrib & 0170000) == 0120000)
		*win32_attrib |= 0x400; /* FILE_ATTRIBUTE_REPARSE_POINT */
	/* S_IFDIR */
	else if ((posix_attrib & 0170000) == 0040000)
		*win32_attrib |= 0x10; /* FILE_ATTRIBUTE_DIRECTORY */
	/* S_IFREG */
	else
		*win32_attrib |= 0x80; /* FILE_ATTRIBUTE_NORMAL */

	return true;
}

bool mz_zip_attrib_win32_to_posix(uint32_t win32_attrib, uint32_t* posix_attrib)
{
	if (posix_attrib == NULL)
		return false;

	*posix_attrib = 0000444; /* S_IRUSR | S_IRGRP | S_IROTH */
	/* FILE_ATTRIBUTE_READONLY */
	if ((win32_attrib & 0x01) == 0)
		*posix_attrib |= 0000222; /* S_IWUSR | S_IWGRP | S_IWOTH */
	/* FILE_ATTRIBUTE_REPARSE_POINT */
	if ((win32_attrib & 0x400) == 0x400)
		*posix_attrib |= 0120000; /* S_IFLNK */
	/* FILE_ATTRIBUTE_DIRECTORY */
	else if ((win32_attrib & 0x10) == 0x10)
		*posix_attrib |= 0040111; /* S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH */
	else
		*posix_attrib |= 0100000; /* S_IFREG */

	return true;
}

/***************************************************************************/

bool mz_zip_extrafield_find(FileStream* stream, uint16_t type, int32_t max_seek, uint16_t* length)
{
	bool     noerr = true;
	uint16_t field_type = 0;
	uint16_t field_length = 0;

	if (max_seek < 4)
		return false;

	do
	{
		noerr = fsReadFromStream(stream, &field_type, sizeof(field_type)) == sizeof(field_type);
		if (noerr)
			noerr = fsReadFromStream(stream, &field_length, sizeof(field_length)) == sizeof(field_length);
		if (!noerr)
			break;

		if (type == field_type)
		{
			if (length != NULL)
				*length = field_length;
			return true;
		}

		max_seek -= field_length - 4;
		if (max_seek < 0)
			return false;

		noerr = fsSeekStream(stream, SBO_CURRENT_POSITION, field_length);
	} while (noerr);

	return false;
}

bool mz_zip_extrafield_contains(const uint8_t* extrafield, int32_t extrafield_size, uint16_t type, uint16_t* length)
{
	FileStream file_extra_stream;
	bool       noerr = true;

	if (extrafield == NULL || extrafield_size == 0)
		return false;

	fsOpenStreamFromMemory(extrafield, extrafield_size, FM_READ, false, &file_extra_stream);

	noerr = mz_zip_extrafield_find(&file_extra_stream, type, extrafield_size, length);

	fsCloseStream(&file_extra_stream);

	return noerr;
}

bool mz_zip_extrafield_read(FileStream* stream, uint16_t* type, uint16_t* length)
{
	bool noerr = true;
	if (type == NULL || length == NULL)
		return false;
	noerr = fsReadFromStream(stream, type, sizeof(*type)) == sizeof(*type);
	if (noerr)
		noerr = fsReadFromStream(stream, length, sizeof(*length)) == sizeof(*length);
	return noerr;
}

bool mz_zip_extrafield_write(FileStream* stream, uint16_t type, uint16_t length)
{
	bool noerr = true;
	noerr = fsWriteToStream(stream, &type, sizeof(type)) == sizeof(type);
	if (noerr)
		noerr = fsWriteToStream(stream, &length, sizeof(length)) == sizeof(length);
	return noerr;
}

/***************************************************************************/

static bool mz_zip_invalid_date(const struct tm* ptm)
{
#define datevalue_in_range(min, max, value) ((min) <= (value) && (value) <= (max))
	return (
		!datevalue_in_range(0, 127 + 80, ptm->tm_year) || /* 1980-based year, allow 80 extra */
		!datevalue_in_range(0, 11, ptm->tm_mon) || !datevalue_in_range(1, 31, ptm->tm_mday) || !datevalue_in_range(0, 23, ptm->tm_hour) ||
		!datevalue_in_range(0, 59, ptm->tm_min) || !datevalue_in_range(0, 59, ptm->tm_sec));
#undef datevalue_in_range
}

static void mz_zip_dosdate_to_raw_tm(uint64_t dos_date, struct tm* ptm)
{
	uint64_t date = (uint64_t)(dos_date >> 16);

	ptm->tm_mday = (uint16_t)(date & 0x1f);
	ptm->tm_mon = (uint16_t)(((date & 0x1E0) / 0x20) - 1);
	ptm->tm_year = (uint16_t)(((date & 0x0FE00) / 0x0200) + 80);
	ptm->tm_hour = (uint16_t)((dos_date & 0xF800) / 0x800);
	ptm->tm_min = (uint16_t)((dos_date & 0x7E0) / 0x20);
	ptm->tm_sec = (uint16_t)(2 * (dos_date & 0x1f));
	ptm->tm_isdst = -1;
}

bool mz_zip_dosdate_to_tm(uint64_t dos_date, struct tm* ptm)
{
	if (ptm == NULL)
		return false;

	mz_zip_dosdate_to_raw_tm(dos_date, ptm);

	if (mz_zip_invalid_date(ptm))
	{
		/* Invalid date stored, so don't return it */
		memset(ptm, 0, sizeof(struct tm));
		return false;
	}
	return true;
}

time_t mz_zip_dosdate_to_time_t(uint64_t dos_date)
{
	struct tm ptm;
	mz_zip_dosdate_to_raw_tm(dos_date, &ptm);
	return mktime(&ptm);
}

bool mz_zip_time_t_to_tm(time_t unix_time, struct tm* ptm)
{
	struct tm* ltm;
	if (ptm == NULL)
		return false;
	ltm = localtime(&unix_time);
	if (ltm == NULL)
	{ /* Returns a 1900-based year */
		/* Invalid date stored, so don't return it */
		memset(ptm, 0, sizeof(struct tm));
		return false;
	}
	memcpy(ptm, ltm, sizeof(struct tm));
	return true;
}

uint32_t mz_zip_time_t_to_dos_date(time_t unix_time)
{
	struct tm ptm;
	mz_zip_time_t_to_tm(unix_time, &ptm);
	return mz_zip_tm_to_dosdate((const struct tm*)&ptm);
}

uint32_t mz_zip_tm_to_dosdate(const struct tm* ptm)
{
	struct tm fixed_tm;

	/* Years supported: */

	/* [00, 79]      (assumed to be between 2000 and 2079) */
	/* [80, 207]     (assumed to be between 1980 and 2107, typical output of old */
	/*                software that does 'year-1900' to get a double digit year) */
	/* [1980, 2107]  (due to format limitations, only years 1980-2107 can be stored.) */

	memcpy(&fixed_tm, ptm, sizeof(struct tm));
	if (fixed_tm.tm_year >= 1980) /* range [1980, 2107] */
		fixed_tm.tm_year -= 1980;
	else if (fixed_tm.tm_year >= 80) /* range [80, 207] */
		fixed_tm.tm_year -= 80;
	else /* range [00, 79] */
		fixed_tm.tm_year += 20;

	if (mz_zip_invalid_date(&fixed_tm))
		return 0;

	return (((uint32_t)fixed_tm.tm_mday + (32 * ((uint32_t)fixed_tm.tm_mon + 1)) + (512 * (uint32_t)fixed_tm.tm_year)) << 16) |
		   (((uint32_t)fixed_tm.tm_sec / 2) + (32 * (uint32_t)fixed_tm.tm_min) + (2048 * (uint32_t)fixed_tm.tm_hour));
}

bool mz_zip_ntfs_to_unix_time(uint64_t ntfs_time, time_t* unix_time)
{
	*unix_time = (time_t)((ntfs_time - 116444736000000000LL) / 10000000);
	return true;
}

bool mz_zip_unix_to_ntfs_time(time_t unix_time, uint64_t* ntfs_time)
{
	*ntfs_time = ((uint64_t)unix_time * 10000000) + 116444736000000000LL;
	return true;
}

/***************************************************************************/

int32_t mz_zip_path_compare(const char* path1, const char* path2, uint8_t ignore_case)
{
	do
	{
		if ((*path1 == '\\' && *path2 == '/') || (*path2 == '\\' && *path1 == '/'))
		{
			/* Ignore comparison of path slashes */
		}
		else if (ignore_case)
		{
			if (tolower(*path1) != tolower(*path2))
				break;
		}
		else if (*path1 != *path2)
		{
			break;
		}

		path1 += 1;
		path2 += 1;
	} while (*path1 != 0 && *path2 != 0);

	if (ignore_case)
		return (int32_t)(tolower(*path1) - tolower(*path2));

	return (int32_t)(*path1 - *path2);
}

/***************************************************************************/

const char* mz_zip_get_compression_method_string(int32_t compression_method)
{
	const char* method = "?";
	switch (compression_method)
	{
		case MZ_COMPRESS_METHOD_STORE: method = "stored"; break;
		case MZ_COMPRESS_METHOD_DEFLATE: method = "deflate"; break;
		case MZ_COMPRESS_METHOD_BZIP2: method = "bzip2"; break;
		case MZ_COMPRESS_METHOD_LZMA: method = "lzma"; break;
		case MZ_COMPRESS_METHOD_XZ: method = "xz"; break;
		case MZ_COMPRESS_METHOD_ZSTD: method = "zstd"; break;
	}
	return method;
}

/***************************************************************************/
