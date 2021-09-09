/************************************************************************************

Filename    :   OVR_DebugMutex.h
Content     :   Implements wrappers for std::mutex and that allows tracking of which
                threads a mutex is locked and unlocked from.
                Can also catch cases where mutexes are:
                - locked / unlocked from different threads
                - used before construction
                - used after destruction
                - destructed while locked
Created     :   May 12, 2020
Authors     :   Jonathan Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "OVR_LogUtils.h"

#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h> // for gettid()

#include <cstdint>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <stdarg.h>

// define OVR_ENABLE_MUTEX_DEBUGGING before including this header to enable mutex debugging
// for any mutexes wrapped in the MUTEX wrappers

// Include the following line before the OVR_DebugMutex.h header to show when mutexes are locked and
// unlocked, and from which threads. #define OVR_DEBUG_MUTEX_VERBOSE

#if defined(OVR_ENABLE_MUTEX_DEBUGGING)

#define DECLARE_MUTEX(mutex_name_) OVR::debug_mutex::DebugStdMutex mutex_name_

#define INIT_MUTEX(mutex_name_, registry_) mutex_name_(registry_, #mutex_name_, __FILE__, __LINE__)

#define LOCK_MUTEX(mutex_name_) mutex_name_.lock(__FILE__, __LINE__)

#define UNLOCK_MUTEX(mutex_name_) mutex_name_.unlock(__FILE__, __LINE__)

#define LOCK_GUARD_MUTEX(mutex_name_) \
    OVR::debug_mutex::LockGuardMutex lock_guard_##mutex_name_(mutex_name_, __FILE__, __LINE__)

#else

#define DECLARE_MUTEX(mutex_name_) std::mutex mutex_name_
#define INIT_MUTEX(mutex_name_, registry_) mutex_name_()
#define LOCK_MUTEX(mutex_name_) mutex_name_.lock()
#define UNLOCK_MUTEX(mutex_name_) mutex_name_.unlock()
#define LOCK_GUARD_MUTEX(mutex_name_) std::lock_guard lock_guard_##mutex_name_(mutex_name_)

#endif

namespace OVR {

namespace debug_mutex {

constexpr int MAX_THREAD_NAME_LEN = 17;

inline const char* GetCurrentThreadName(char threadName[MAX_THREAD_NAME_LEN]) {
    // null terminate at first byte in case prctl does nothing
    threadName[0] = '\0';
    prctl(PR_GET_NAME, &threadName[0], 0L, 0L, 0L);
    // null terminate at last byte in case prctl doesn't property null-terminate
    threadName[MAX_THREAD_NAME_LEN - 1] = '\0';
    return &threadName[0];
}

inline bool
Error(const char* mutexName, const char* fileName, const int32_t lineNumber, const char* fmt, ...) {
    va_list argPtr;

    // determine the size of the message
    va_start(argPtr, fmt);
    const int msgLen = vsnprintf(nullptr, 0, fmt, argPtr);
    va_end(argPtr);

    // allocate a variable-length array of the right size
    char msg[msgLen + 1];
    va_start(argPtr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argPtr);
    va_end(argPtr);

    // fail with the full message
    OVR_FAIL("Mutex error: '%s' - %s(%i): %s", mutexName, fileName, abs(lineNumber), msg);
    return false;
}

// DJB2 string hash function to generate a continuous hash from a string and an a value type
template <typename type_>
static uint64_t GenHash(const char* str1, const type_ value) {
    static const uint64_t HASH_SEED = 5381;

    if (str1 == nullptr) {
        return 0ULL;
    }

    auto HashString = [](uint64_t& hash, const void* buffer, const size_t len) {
        const uint8_t* d = reinterpret_cast<const uint8_t*>(buffer);

        for (size_t i = 0; i < len; ++i) {
            hash = ((hash << 5) + hash) + d[i];
        }
    };

    uint64_t hash = HASH_SEED;
    HashString(hash, str1, strlen(str1));
    HashString(hash, &value, sizeof(type_));

    return hash;
}

struct HandleHashFn {
    uint64_t operator()(const uint64_t& handle) const {
        return handle;
    }
};

//==============================================================
// MutexRef
//==============================================================
class MutexRef {
   public:
    enum OpType { CONSTRUCT, LOCK, WAIT, UNLOCK, DESTRUCT };

    MutexRef(const uint64_t handle, const char* fileName, const int32_t lineNumber, const OpType op)
        : handle_(handle), fileName_(fileName), lineNumber_(lineNumber), op_(op), count_(0) {}

    uint64_t handle_; // handle generated from file name and line number

    std::string fileName_; // file name where the mutex operation was initiated
    int32_t lineNumber_; // line number where the mutex operation was initiated
    OpType op_; // operation type performed at the associated file and line
    int32_t count_; // number of times this operation was performed
};

//==============================================================
// MutexRecord
//==============================================================
class MutexRecord {
   public:
    static const size_t MAX_MUTEX_NAME_LEN = 32;

    MutexRecord(const uint64_t handle, const char* name)
        : handle_(handle), name_(name), opCount_(0), constructCount_(0), lockCount_(0) {}
    ~MutexRecord() {}

    bool IsConstructed() {
        return (constructCount_ % 2) == 1;
    }
    bool IsLocked() {
        return (lockCount_ % 2) == 1;
    }

    void AddReference(const char* fileName, const int32_t lineNumber, const MutexRef::OpType op) {
        // OVR_LOG("AddReference: %s:(%i) - op = %i", fileName, lineNumber, op);
        MutexRef* ref = nullptr;
        const int32_t tid = gettid();
        const uint64_t refHash = GenHash(fileName, lineNumber);
        auto it = opMap_.find(refHash);
        if (it == opMap_.end()) {
            // first time we've hit this reference, so add a new reference
            char threadName[17];
            GetCurrentThreadName(threadName);
            ref = new MutexRef(refHash, fileName, lineNumber, op);
            opMap_.insert({refHash, ref});
        } else {
            ref = it->second;
            if (op != ref->op_ &&
                !((op == MutexRef::LOCK && ref->op_ == MutexRef::WAIT) ||
                  (op == MutexRef::WAIT && ref->op_ == MutexRef::LOCK))) {
                // if this happens we've made a logic error somewhere
                Error(name_.c_str(), fileName, lineNumber, "op(%i) != ref->op(%i)!", op, ref->op_);
            }
        }

        char curThreadName[MAX_THREAD_NAME_LEN];
        GetCurrentThreadName(curThreadName);
        bool updateThread = true;

        switch (op) {
            case MutexRef::UNLOCK:
                if (threadId_ != tid) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) tried to unlock a mutex locked from thread %s(%i)!",
                        curThreadName,
                        tid,
                        threadName_,
                        threadId_);
                } else if (!IsConstructed()) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) locked mutex after destruction!",
                        curThreadName,
                        tid);
                } else if (!IsLocked()) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) tried to unlock a muted that is not locked!",
                        curThreadName,
                        tid);
                }
                lockCount_++;
#if defined(OVR_DEBUG_MUTEX_VERBOSE)
                OVR_LOG(
                    "Mutex '%s' - %s:(%i) UNLOCKED by thread %s(%i)",
                    name_.c_str(),
                    fileName,
                    lineNumber,
                    curThreadName,
                    tid);
#endif
                break;
            case MutexRef::LOCK:
                if (!IsConstructed()) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) tried to lock an unconstructed mutex!",
                        curThreadName,
                        tid);
                }
                if (IsLocked() && threadId_ == tid) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) tried to lock a mutex that it is already locking!",
                        curThreadName,
                        tid);
                }
                lockCount_++;
#if defined(OVR_DEBUG_MUTEX_VERBOSE)
                OVR_LOG(
                    "Mutex '%s' - %s:(%i) LOCKED by thread %s(%i)",
                    name_.c_str(),
                    fileName,
                    lineNumber,
                    curThreadName,
                    tid);
#endif
                break;
            case MutexRef::DESTRUCT:
                if (!IsConstructed()) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) tried to destruct an uncontructed mutex!",
                        curThreadName,
                        tid);
                }
                if (IsLocked()) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) destructed while mutex is locked!",
                        curThreadName,
                        tid);
                }
                constructCount_++;
                break;
            case MutexRef::CONSTRUCT:
                if (IsConstructed()) {
                    Error(
                        name_.c_str(),
                        fileName,
                        lineNumber,
                        "thread %s(%i) tried to construct a mutex already constructed by thread %s(%i)!",
                        curThreadName,
                        tid,
                        threadName_,
                        threadId_);
                }
                constructCount_++;
                break;
            case MutexRef::WAIT:
                updateThread = false;
#if defined(OVR_DEBUG_MUTEX_VERBOSE)
                OVR_LOG(
                    "Mutex '%s' - %s:(%i) thread %s(%i) WAITING on thread %s(%i)",
                    name_.c_str(),
                    fileName,
                    lineNumber,
                    curThreadName,
                    tid,
                    threadName_,
                    threadId_);
#endif
                break;
            default:
                Error(name_.c_str(), fileName, lineNumber, "Unknown mutex op %i!", op);
                break;
        }
        ref->count_++;
        opCount_++;
        if (updateThread) {
            threadId_ = tid;
            OVR_strcpy(threadName_, sizeof(threadName_), curThreadName);
        }
    }

    uint64_t handle_; // handle genereated from file name and line number
    std::string name_; // name of the mutex variable
    uint32_t opCount_; // increments every time an operation is performed on this mutex
    int32_t constructCount_; // increments on construction, decrements on destruction (odd ==
                             // constructed, even == destructed)
    int32_t
        lockCount_; // increments on lock, decrements on unlock (odd == locked, even == unlocked)

    char threadName_[MAX_THREAD_NAME_LEN]; // name of the thread that performed the last lock
    int32_t threadId_; // thread id of the last thread that locked the mutex

    // for each call to a mutex function (construct, destruct, lock, unlock) the MutexRecord
    // will save a MutexRef.
    std::unordered_map<uint64_t, MutexRef*, HandleHashFn> opMap_;
};

//==============================================================
// MutexRegistry
// The registry keeps track of multiple mutexes. The mutex tracking persists
// beyond the lifetime of the mutex so that the registry can be used to check
// for mutexes that are used post-destruction.
//==============================================================
class MutexRegistry {
   public:
    enum MutexOp { MUTEX_CONSTRUCT, MUTEX_LOCK, MUTEX_UNLOCK, MUTEX_DESTRUCT };

    enum MutexError {

    };

    MutexRegistry() {}
    ~MutexRegistry() {}

    void AddReference(
        const void* mutexPtr,
        const char* mutexName,
        const char* fileName,
        const int32_t lineNumber,
        const MutexRef::OpType op) {
        const uint64_t handle = reinterpret_cast<uint64_t>(mutexPtr);
        MutexRecord* mutexRec = nullptr;

        std::lock_guard loc(mutex_);
        auto it = mutexMap_.find(handle);
        if (it == mutexMap_.end()) {
            // OVR_LOG("Creating new MutexRecord for '%s', handle = %" PRIu64 "...", mutexName,
            // handle);
            if (op != MutexRef::CONSTRUCT) {
                const int32_t tid = gettid();
                char threadName[17];

                Error(
                    mutexName,
                    fileName,
                    lineNumber,
                    "thread %s(%i) referenced mutex before construction!",
                    GetCurrentThreadName(threadName),
                    tid);
            }
            mutexRec = new MutexRecord(handle, mutexName);
            mutexMap_.insert({handle, mutexRec});
        } else {
            // OVR_LOG("Found MutexRecord for '%s', handle = %" PRIu64 "...", mutexName, handle);
            mutexRec = it->second;
        }
        mutexRec->AddReference(fileName, lineNumber, op);
    }

   private:
    std::mutex mutex_;
    std::unordered_map<uint64_t, MutexRecord*, HandleHashFn> mutexMap_;
};

//==============================================================
// DebugStdMutex
//==============================================================
class DebugStdMutex {
   public:
    static const int MUTEX_MAX_NAME_LEN = 32;
    static const int THREAD_MAX_NAME_LEN = 32;

    DebugStdMutex(
        MutexRegistry& mutexRegistry,
        const char* mutexName,
        const char* fileName,
        const int32_t lineNumber)
        : mutexRegistry_(mutexRegistry), fileName_(fileName), lineNumber_(lineNumber) {
        std::lock_guard debugLock(debugMutex_);
        OVR_strcpy(mutexName_, sizeof(mutexName_), mutexName);
        mutexRegistry_.AddReference(
            this, &mutexName_[0], fileName, lineNumber, MutexRef::CONSTRUCT);
    }

    ~DebugStdMutex() {
        std::lock_guard debugLock(debugMutex_);
        mutexRegistry_.AddReference(
            this, &mutexName_[0], fileName_.c_str(), -lineNumber_, MutexRef::DESTRUCT);
    }

    void lock(const char* fileName, const int32_t lineNumber) {
        std::lock_guard debugLock(debugMutex_);

        if (!mutex_.try_lock()) {
            OVR_LOG("waiting");
            // the mutex is locked by something else, so this thread would simply end up waiting
            mutexRegistry_.AddReference(this, &mutexName_[0], fileName, lineNumber, MutexRef::WAIT);
            // block on this mutex (assuming it didn't just get released by the locking thread after
            // we unlocked the debugMutex)
            OVR_LOG("lock mutex_");
            mutex_.lock();
            // after waiting, we acquire the lock
            mutexRegistry_.AddReference(this, &mutexName_[0], fileName, lineNumber, MutexRef::LOCK);
        } else {
            mutexRegistry_.AddReference(this, &mutexName_[0], fileName, lineNumber, MutexRef::LOCK);
        }
    }

    void unlock(const char* fileName, const int32_t lineNumber) {
        // std::lock_guard debugLock(debugMutex_);
        mutexRegistry_.AddReference(this, &mutexName_[0], fileName, lineNumber, MutexRef::UNLOCK);
        mutex_.unlock();
    }

   private:
    MutexRegistry& mutexRegistry_;
    std::mutex mutex_;
    char mutexName_[MUTEX_MAX_NAME_LEN]; // name of mutex
    std::string fileName_;
    int32_t lineNumber_;

    // this mutex wraps the normal lock / unlock calls to ensure the mutex tracking operations are
    // treated atomically
    std::mutex debugMutex_;
};

//==============================================================
// LockGuardMutex
//==============================================================
class LockGuardMutex {
   public:
    LockGuardMutex(DebugStdMutex& mutex, const char* fileName, const int32_t lineNumber)
        : mutex_(mutex), fileName_(fileName), lineNumber_(lineNumber) {
        mutex_.lock(fileName, lineNumber);
    }
    ~LockGuardMutex() {
        // we pass a -lineNumber because we cannot get a real line number for the destructor
        // (since there is no macro explictly coded at the destructor location) and we do
        // not want the guard destructor's unlock reference to be the same as its constructor.
        mutex_.unlock(fileName_.c_str(), -lineNumber_);
    }

   private:
    DebugStdMutex& mutex_;
    std::string fileName_;
    int32_t lineNumber_;
};

} // namespace debug_mutex

} // namespace OVR
