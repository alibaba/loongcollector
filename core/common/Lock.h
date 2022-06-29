/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <mutex>
#include <boost/thread.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>
#ifdef _MSC_VER
#define NOGDI
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace logtail {

// Copy from boost::detail::spinlock but add initialization.
class SpinLock {
    std::atomic_flag v_ = ATOMIC_FLAG_INIT;

public:
    SpinLock() {}

    bool try_lock() { return !v_.test_and_set(std::memory_order_acquire); }

    void lock() {
        for (unsigned k = 0; !try_lock(); ++k) {
            boost::detail::yield(k);
        }
    }

    void unlock() { v_.clear(std::memory_order_release); }
};
using ScopedSpinLock = std::lock_guard<SpinLock>;

// TODO(baoze.yyh): there is a potential problem, on Windows, because the slim reader/writer
// lock doesn't supply any methods to set r/w preferences, it may take some (unknown) time
// for writer to accquire lock from readers(s). So, the HoldOn/Resume with ReadWriteLock
// can not stop/resume immediately, which might cause some problems related to timeout.
class ReadWriteLock {
#ifdef _MSC_VER
    SRWLOCK mLock;
#else
    pthread_rwlock_t mLock;
#endif

public:
    enum Mode { PREFER_READER, PREFER_WRITER };

    explicit ReadWriteLock(Mode mode = PREFER_WRITER) {
#ifdef _MSC_VER
        InitializeSRWLock(&mLock);
#else
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        if (PREFER_WRITER == mode) {
            pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
        }
        pthread_rwlock_init(&mLock, &attr);
        pthread_rwlockattr_destroy(&attr);
#endif
    }

    ~ReadWriteLock() {
#ifndef _MSC_VER
        pthread_rwlock_destroy(&mLock);
#endif
    }

    void lock_shared() {
#ifdef _MSC_VER
        AcquireSRWLockShared(&mLock);
#else
        pthread_rwlock_rdlock(&mLock);
#endif
    }

    void unlock_shared() {
#ifdef _MSC_VER
        ReleaseSRWLockShared(&mLock);
        Sleep(1);
#else
        pthread_rwlock_unlock(&mLock);
#endif
    }

    void lock() {
#ifdef _MSC_VER
        AcquireSRWLockExclusive(&mLock);
#else
        pthread_rwlock_wrlock(&mLock);
#endif
    }

    void unlock() {
#ifdef _MSC_VER
        ReleaseSRWLockExclusive(&mLock);
#else
        pthread_rwlock_unlock(&mLock);
#endif
    }
};

using ReadLock = boost::shared_lock<ReadWriteLock>;
using WriteLock = boost::unique_lock<ReadWriteLock>;

using PTMutex = std::mutex;
using PTScopedLock = std::lock_guard<PTMutex>;

} // namespace logtail
