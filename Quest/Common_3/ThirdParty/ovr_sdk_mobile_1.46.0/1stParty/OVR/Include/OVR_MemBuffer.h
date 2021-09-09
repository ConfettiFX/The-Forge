/************************************************************************************

Filename    :   MemBuffer.h
Content     :	Memory buffer.
Created     :	May 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#ifndef MEMBUFFER_H
#define MEMBUFFER_H

#include "OVR_Types.h"

#include <stdio.h>

#include <inttypes.h>
#include <cstddef> // for NULL
#include <stdlib.h>
#include <vector>

#include <sys/types.h>

#include <fcntl.h>
#if defined(OVR_OS_ANDROID)
#include <unistd.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <sys/stat.h>
#pragma GCC diagnostic pop
#else
#include <sys/stat.h>
#endif

#include "OVR_LogUtils.h"

namespace OVR {
// This does NOT free the memory on delete, it is just a wrapper around
// memory, and can be copied freely for passing / returning as a value.
// This does NOT have a virtual destructor, so if a copy is made of
// a derived version, it won't cause it to be freed.
class MemBuffer {
   public:
    MemBuffer() : Buffer(0), Length(0) {}
    explicit MemBuffer(int length) : Buffer(new char[length]), Length(length) {}
    MemBuffer(void* buffer, int length) : Buffer(buffer), Length(length) {}
    ~MemBuffer() {}

    // Calls Free() on Buffer and sets it to NULL and lengrh to 0
    void FreeData() {
        if (Buffer != NULL) {
            delete[] static_cast<char*>(Buffer);
            Buffer = NULL;
        }
        Length = 0;
    }

    bool WriteToFile(const char* filename) {
        OVR_LOG("Writing %i bytes to %s\n", Length, filename);
        FILE* f = fopen(filename, "wb");
        if (f != NULL) {
            fwrite(Buffer, Length, 1, f);
            fclose(f);
            return true;
        } else {
            OVR_LOG("MemBuffer::WriteToFile failed to write to %s\n", filename);
        }
        return false;
    }

    void* Buffer;
    int Length;
};

// This DOES free the memory on delete.
class MemBufferFile : public MemBuffer {
   public:
    enum eNoInit { NoInit };

    explicit MemBufferFile(const char* filename) {
        LoadFile(filename);
    }

    explicit MemBufferFile(
        eNoInit const noInit) // use this constructor as MemBufferFile( NoInit ) -- this takes a
                              // parameters so you still can't defalt-construct by accident
    {
        OVR_UNUSED(noInit);
    }

    virtual ~MemBufferFile() {
        FreeData();
    }

    bool LoadFile(const char* filename) {
        FreeData();
#if !defined(OVR_OS_ANDROID)
        FILE* f = fopen(filename, "rb");
        if (!f) {
            OVR_LOG("Couldn't open %s\n", filename);
            return false;
        }
        fseek(f, 0, SEEK_END);
        Length = ftell(f);
        fseek(f, 0, SEEK_SET);
        Buffer = malloc(Length);
        const size_t readRet = fread((unsigned char*)Buffer, Length, 1, f);
        fclose(f);
        if (readRet != 1) {
            OVR_LOG("Only read %zu of %i bytes in %s\n", readRet, Length, filename);
            FreeData();
            return false;
        }
        return true;
#else
        // Using direct IO gives read speeds of 200 - 290 MB/s,
        // versus 130 - 170 MB/s with buffered stdio on a background thread.
        const int fd = open(filename, O_RDONLY, 0);
        if (fd < 0) {
            OVR_LOG("Couldn't open %s\n", filename);
            return false;
        }
        struct stat buf;
        if (-1 == fstat(fd, &buf)) {
            close(fd);
            OVR_LOG("Couldn't fstat %s\n", filename);
            return false;
        }
        Length = (int)buf.st_size;
        Buffer = malloc(Length);
        const size_t readRet = read(fd, (unsigned char*)Buffer, Length);
        close(fd);
        if (readRet != (size_t)Length) {
            OVR_LOG("Only read %zu of %i bytes in %s\n", readRet, Length, filename);
            FreeData();
            return false;
        }
        return true;
#endif
    }

    // Moves the data to a new MemBuffer that won't
    // be deleted on destruction, removing it from the
    // MemBufferFile.
    MemBuffer ToMemBuffer() {
        MemBuffer mb(Buffer, Length);
        Buffer = NULL;
        Length = 0;
        return mb;
    }
};

//==============================================================
// MemBufferT
//
// This allocates memory on construction and frees the memory on delete.
// On copy assignment or copy construct any existing pointer in the destination
// is freed, the pointer from the source is assigned to the destination and
// the pointer in the source is cleared.
template <class C>
class MemBufferT {
   public:
    MemBufferT() : Buffer(nullptr), Size(0) {}

    // allocates a buffer of the specified size.
    explicit MemBufferT(size_t const size) : Buffer(nullptr), Size(size) {
        // FIXME: this can throw on a failed allocation and we are currently building
        // without exception handling. This means we can't catch the throw and we'll
        // get an abort. We either need to turn on exception handling or do our own
        // in-place new and delete.
        Buffer = new C[size];
    }

    // take ownership of an already allocated buffer
    explicit MemBufferT(void*& buffer, size_t const size)
        : Buffer(static_cast<C*>(buffer)), Size(size) {
        buffer = nullptr;
    }

    explicit MemBufferT(const std::vector<C>& v) : MemBufferT(v.size()) {
        memcpy(Buffer, v.data(), v.size());
    }

    // frees the buffer on deconstruction
    ~MemBufferT() {
        Free();
    }

    // returns a const pointer to the buffer
    operator C const *() const {
        return Buffer;
    }

    // returns a non-const pointer to the buffer
    operator C*() {
        return Buffer;
    }

    size_t GetSize() const {
        return Size;
    }

    bool IsNull() const {
        return Buffer == NULL;
    }

    // assignment operator
    MemBufferT& operator=(MemBufferT& other) {
        if (&other == this) {
            return *this;
        }

        Free(); // free existing data before copying

        Buffer = other.Buffer;
        Size = other.Size;
        other.Buffer = 0;
        other.Size = 0;

        return *this;
    }

    // frees any existing buffer and allocates a new buffer of the specified size
    void Realloc(size_t size) {
        Free();

        Buffer = new C[size];
        Size = size;
    }

    // This is for interop with code that uses void * to allocate a raw buffer
    void TakeOwnershipOfBuffer(void*& buffer, size_t const size) {
        Free();
        Buffer = static_cast<C*>(buffer);
        buffer = nullptr;
        Size = size;
    }

    // This is for interop with code that uses void * to allocate a raw buffer
    // and which uses a signed integer for the buffer size.
    void TakeOwnershipOfBuffer(void*& buffer, int const size) {
        Free();
        Buffer = static_cast<C*>(buffer);
        buffer = nullptr;
        Size = static_cast<size_t>(size);
    }

    // Explicitly transfer ownership of the pointer to the caller.
    void TransferOwnershipOfBuffer(void*& outBuffer, size_t& outSize) {
        outBuffer = Buffer;
        outSize = Size;
        Buffer = nullptr;
        Size = 0;
    }

   private:
    C* Buffer;
    size_t Size;

   private:
    // Private copy constructor to prevent MemBufferT objects from being accidentally
    // passed by value. This will also prevent copy-assignment from a temporary object:
    // MemBufferT< uint8_t > buffer = MemBufferT< uint8_t >( size ); // this will fail
    MemBufferT(MemBufferT& other);

    // frees the existing buffer
    void Free() {
        delete[] Buffer;
        Buffer = nullptr;
        Size = 0;
    }
};

} // namespace OVR

#endif // MEMBUFFER_H
