/************************************************************************************

Filename    :   OVR_Lockless.h
Content     :   Lock-less classes for producer/consumer communication
Created     :   November 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_Lockless_h
#define OVR_Lockless_h

#include <atomic>
#include <memory>
#include <cstring> // memcpy

#if defined(OVR_OS_WIN32) || defined(_WIN32) || defined(_WIN64)
#define NOMINMAX // stop Windows.h from redefining min and max and breaking std::min / std::max
#include <windows.h> // for MemoryBarrier
#endif

namespace OVR {

// ***** LocklessUpdater

// For single producer cases where you only care about the most recent update, not
// necessarily getting every one that happens (vsync timing, SensorFusion updates).
//
// This is multiple consumer safe.
//
// TODO: This is Android specific

template <class T>
class LocklessUpdater {
   public:
    LocklessUpdater() : UpdateBegin(0), UpdateEnd(0) {}

    LocklessUpdater(const LocklessUpdater& other) : LocklessUpdater() {
        SetState(other.GetState());
    }

    LocklessUpdater& operator=(const LocklessUpdater& other) {
        if (this != &other) {
            T state;
            other.GetState(state);
            this->SetState(state);
        }
        return *this;
    }

    T GetState() const {
        // This is the original code path with the copy on return, which exists for backwards
        // compatibility To avoid this copy, use GetState below
        T state;
        GetState(state);
        return state;
    }

    void GetState(T& state) const {
        // Copy the state out, then retry with the alternate slot
        // if we determine that our copy may have been partially
        // stepped on by a new update.
        int begin, end, final;

        for (;;) {
            end = UpdateEnd.load(std::memory_order_acquire);
            state = Slots[end & 1];
// Manually insert an memory barrier here in order to ensure that
// memory access between Slots[] and UpdateBegin are properly ordered
#if defined(OVR_CC_MSVC) || defined(_WIN32) || defined(_WIN64)
            MemoryBarrier();
#else
            __sync_synchronize();
#endif
            begin = UpdateBegin.load(std::memory_order_acquire);
            if (begin == end) {
                return;
            }

            // The producer is potentially blocked while only having partially
            // written the update, so copy out the other slot.
            state = Slots[(begin & 1) ^ 1];
#if defined(OVR_CC_MSVC) || defined(_WIN32) || defined(_WIN64)
            MemoryBarrier();
#else
            __sync_synchronize();
#endif
            final = UpdateBegin.load(std::memory_order_acquire);
            if (final == begin) {
                return;
            }

            // The producer completed the last update and started a new one before
            // we got it copied out, so try fetching the current buffer again.
        }
    }

    void SetState(const T& state) {
        const int slot = UpdateBegin.fetch_add(1, std::memory_order_seq_cst) & 1;
        // Write to (slot ^ 1) because fetch_add returns 'previous' value before add.
        Slots[slot ^ 1] = state;
        UpdateEnd.fetch_add(1, std::memory_order_seq_cst);
    }

    std::atomic<int> UpdateBegin;
    std::atomic<int> UpdateEnd;
    T Slots[2];
};

// This style of lockless updater is more like the one in LocklessBuffer.h used in the Tracking
// Service. We have encountered issues with LocklessUpdater in debug builds that do not appear in
// LocklessUpdater2. LocklessUpdater2 is simpler in that it has only one count variable, which is
// only updated by SetState(). The updates happen both before and after writes to Slot memory, so
// that the lsb of count indicates whether a write is in progress, and (count >> 1) is used to
// derive the read and write indices.

// This class is a workalike replacement for LocklessUpdater, but it has a different memory layout
// and protocol, so all users of the shared memory must be using matching class definitions.

template <class T>
class LocklessUpdater2 {
   public:
    LocklessUpdater2() : Count(0) {}

    LocklessUpdater2(const LocklessUpdater2& other) : LocklessUpdater2() {
        SetState(other.GetState());
    }

    LocklessUpdater2& operator=(const LocklessUpdater2& other) {
        if (this != &other) {
            T state;
            other.GetState(state);
            this->SetState(state);
        }
        return *this;
    }

    T GetState() const {
        // This is the original code path with the copy on return, which exists for backwards
        // compatibility To avoid this copy, use GetState below
        T state;
        GetState(state);
        return state;
    }

    void GetState(T& state) const {
        for (;;) {
            // Atomically load Count into a temporary variable (c), to determine the index
            // of the last-written buffer.
            int c = Count.load(std::memory_order_acquire);

            c &= ~int(1); // mask bit 0 so the differences below work

            //  The last-written buffer is specified by bit 1 of the the variable c.
            const int readIndex = (c >> 1) & 1;

            // read the buffer into the buffer parameter
            state = Slots[readIndex];

            // After copying the data from that buffer, Count is atomically loaded into another
            // temporary variable (c2). If the count changed at all, we know that a write to the
            // buffers occured. Depending on the number of increments between c and c2, we can tell
            // which buffers were written to. Our read is ok as long as the write was to the buffer
            // we were not reading from.
            const int c2 = Count.load(std::memory_order_acquire);

            // * ( c2 - c ) == 0, no writes occured while the buffer was read the data is good.
            // * ( c2 - c ) == 1, a write started, but it was in the buffer that wasn't read, so the
            //   data read from the buffer is good.
            // * ( c2 - c ) == 2, a write ended, but it was in the buffer that wasn't read, so the
            //   data read from the buffer is good.
            // * ( c2 - c ) == 3, a new write began in the buffer that was read, so the buffer is
            //   likely to have partial data in it and must be re-read.
            // * ( c2 - c ) > 3, multiple writes happened, at least one of which overwrote the
            //   buffer that was  read, so the buffer may have partial data and should be re-read.
            if ((c2 - c) < 3) {
                return;
            }
        }
    }

    void SetState(const T& state) {
        int writeIndex = ((Count.fetch_add(1) >> 1) + 1) & 1;

        Slots[writeIndex] = state;

        // memory barrier with release ensures that no write to memory before the fence
        // can be reordered with a memory operation occurring after the fence.
        std::atomic_thread_fence(std::memory_order_release);
        // atomic increment
        Count.fetch_add(1);
    }

    std::atomic<int> Count;
    T Slots[2];
};

struct LocklessDataReaderState {
    int32_t CountMasked = 0;
    int32_t ReadIndex = 0;
    int32_t BufferSize = 0;
};

struct LocklessDataWriterState {
    int32_t WriteIndex = 0;
};

class LocklessDataHelper {
   public:
#ifdef LOCKLESS_DATA_HELPER_CHECK_HASH
    LocklessDataHelper() : Count(0), DataSizes{0, 0}, Hashes{0, 0} {}

    using HashType = uint64_t;

    static HashType HashBytes(const uint8_t* data, const size_t size) {
        HashType hash = 5381;

        for (size_t i = 0; i < size; ++i) {
            hash = ((hash << 5) + hash) + data[i];
        }

        return hash;
    }
#else
    LocklessDataHelper() : Count(0), DataSizes{0, 0} {}
#endif

    inline void GetDataBegin(LocklessDataReaderState& locker) const {
        // Atomically load Count into a temporary variable (locker.CountMasked), to determine the
        // index of the last-written buffer.
        locker.CountMasked = Count.load(std::memory_order_acquire) & ~int32_t(1);
        locker.ReadIndex = (locker.CountMasked >> 1) & 1;
        locker.BufferSize = DataSizes[locker.ReadIndex];
    }

    inline bool GetDataEnd(LocklessDataReaderState& locker) const {
        // After copying the data from that buffer, Count is atomically loaded into another
        // temporary variable (c2). If the count changed at all, we know that a write to the
        // buffers occured. Depending on the number of increments between c and c2, we can tell
        // which buffers were written to. Our read is ok as long as the write was to the buffer
        // we were not reading from.
        const int c2 = Count.load(std::memory_order_acquire);
        // * ( c2 - c ) == 0, no writes occured while the buffer was read the data is good.
        // * ( c2 - c ) == 1, a write started, but it was in the buffer that wasn't read, so the
        //   data read from the buffer is good.
        // * ( c2 - c ) == 2, a write ended, but it was in the buffer that wasn't read, so the
        //   data read from the buffer is good.
        // * ( c2 - c ) == 3, a new write began in the buffer that was read, so the buffer is
        //   likely to have partial data in it and must be re-read.
        // * ( c2 - c ) > 3, multiple writes happened, at least one of which overwrote the
        //   buffer that was  read, so the buffer may have partial data and should be re-read.
        return ((c2 - locker.CountMasked) < 3);
    }

    inline bool GetData(const uint8_t** buffers, uint8_t* outData, int32_t& outSize) const {
        const int32_t inSize = outSize;
        outSize = 0;

        for (;;) {
            LocklessDataReaderState locker;
            GetDataBegin(locker);
            if (locker.BufferSize > inSize) {
                return false;
            }

            std::memcpy(outData, buffers[locker.ReadIndex], locker.BufferSize);
#ifdef LOCKLESS_DATA_HELPER_CHECK_HASH
            const HashType expectedHash = Hashes[locker.ReadIndex];
#endif

            if (GetDataEnd(locker)) {
#ifdef LOCKLESS_DATA_HELPER_CHECK_HASH
                const HashType hash = HashBytes(outData, locker.BufferSize);
                if (hash != expectedHash) {
                    OVR_WARN(
                        "GetData hash mismatch: Size: %i, %llu != %llu",
                        (int)locker.BufferSize,
                        (unsigned long long int)hash,
                        (unsigned long long int)expectedHash);
                    return false;
                }
#endif
                outSize = locker.BufferSize;
                return true;
            }
        }
    }

    inline void SetDataBegin(LocklessDataWriterState& writer) {
        writer.WriteIndex = ((Count.fetch_add(1) >> 1) + 1) & 1;
    }

    inline void
    SetDataEnd(LocklessDataWriterState& writer, const uint8_t* data, const int32_t size) {
        DataSizes[writer.WriteIndex] = size;

#ifdef LOCKLESS_DATA_HELPER_CHECK_HASH
        Hashes[writer.WriteIndex] = HashBytes(data, size);
#endif

        // memory barrier with release ensures that no write to memory before the fence
        // can be reordered with a memory operation occurring after the fence.
        std::atomic_thread_fence(std::memory_order_release);
        // atomic increment
        Count.fetch_add(1);
    }

    inline void SetData(uint8_t** buffers, const uint8_t* data, const int32_t size) {
        LocklessDataWriterState writer;
        SetDataBegin(writer);

        std::memcpy(buffers[writer.WriteIndex], data, size);

        SetDataEnd(writer, data, size);
    }

    std::atomic<int32_t> Count;
    std::atomic<int32_t> DataSizes[2];
#ifdef LOCKLESS_DATA_HELPER_CHECK_HASH
    std::atomic<HashType> Hashes[2];
#endif
};

// Same as LocklessUpdater, but allows you to specify arbitrarily sized copies
template <int32_t MaxBufferSize>
class LocklessDataBuffer {
   public:
    LocklessDataBuffer() {}

    bool GetData(uint8_t* outData, int32_t& outSize) const {
        const uint8_t* buffers[2] = {Slots[0], Slots[1]};

        return Helper.GetData(buffers, outData, outSize);
    }

    void SetData(const uint8_t* data, const int32_t size) {
        uint8_t* buffers[2] = {Slots[0], Slots[1]};
        Helper.SetData(buffers, data, size);
    }

    LocklessDataHelper Helper;
    uint8_t Slots[2][MaxBufferSize];
};

} // namespace OVR

#endif // OVR_Lockless_h
