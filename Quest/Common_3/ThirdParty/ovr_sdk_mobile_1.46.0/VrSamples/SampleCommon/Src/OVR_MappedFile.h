/************************************************************************************

Filename    :   OVR_MappedFile.h
Content     :   Cross-platform memory-mapped file wrapper.
Created     :   May 12, 2014
Authors     :   Chris Taylor

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

/*
    Memory-mapped files are a fairly good compromise between performance and flexibility.

    Compared with asynchronous io, memory-mapped files are:
        + Much easier to implement in a portable way
        + Automatically paged in and out of RAM
        + Automatically read-ahead cached

    When asynch IO is not available or blocking is acceptable then this is a
    great alternative with low overhead and similar performance.

    For random file access, use MappedView with a MappedFile that has been
    opened with random_access = true.  Random access is usually used for a
    database-like file type, which is much better implemented using asynch IO.
*/

namespace OVRFW {

// Read-only memory mapped file
class MappedFile {
    friend class MappedView;

   public:
    MappedFile();
    ~MappedFile();

    // Opens the file for shared read-only access with other applications
    // Returns false on error (file not found, etc)
    bool OpenRead(const char* path, bool read_ahead = false, bool no_cache = false);

    // Creates and opens the file for exclusive read/write access
    bool OpenWrite(const char* path, size_t size);

    void Close();

    bool IsReadOnly() const {
        return ReadOnly;
    }
    size_t GetLength() const {
        return Length;
    }
    bool IsValid() const {
        return (Length != 0);
    }

   private:
    int File;
    bool ReadOnly;
    size_t Length;
};

// View of a portion of the memory mapped file
class MappedView {
   public:
    MappedView();
    ~MappedView();

    bool Open(MappedFile* file); // Returns false on error
    uint8_t* MapView(
        size_t offset = 0,
        uint32_t length = 0); // Returns 0 on error, 0 length means whole file
    void Close();

    bool IsValid() const {
        return (Data != 0);
    }
    size_t GetOffset() const {
        return Offset;
    }
    uint32_t GetLength() const {
        return Length;
    }
    MappedFile* GetFile() {
        return File;
    }
    uint8_t* GetFront() {
        return Data;
    }

   private:
    void* Map;

    MappedFile* File;
    uint8_t* Data;
    size_t Offset;
    uint32_t Length;
};

} // namespace OVRFW
