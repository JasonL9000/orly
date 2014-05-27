/* <base/sharex.h>

   Like a mutex, but with shared- and exclusive-lock modes.

   Copyright 2010-2014 OrlyAtomics, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once

#include <cassert>
#include <utility>

#include <pthread.h>

#include <base/class_traits.h>
#include <base/error_utils.h>

namespace Base {

  /* Like a mutex, but with shared- and exclusive-lock modes. */
  class TSharex {
    NO_COPY(TSharex);
    public:

    /* How do you like your lock? */
    enum TMode { Shared, Exclusive };

    /* RAII locking of the sharex. */
    class TLock {
      NO_COPY(TLock);
      public:

      /* Start out null, meaning not locking anything. */
      TLock() noexcept
          : Sharex(nullptr) {}

      /* Take over the donor lock's sharex, leaving the donor null.  If the
         donor lock is already null, then it stays null and construct as
         null.*/
      TLock(TLock &&that) noexcept {
        assert(this);
        assert(&that);
        Sharex = that.Sharex;
        that.Sharex = nullptr;
      }

      /* Lock the given sharex in shared- or exclusive-mode.  We will not
         return until the lock is granted. */
      TLock(TSharex &sharex, TMode mode) {
        assert(&sharex);
        int (*func)(pthread_rwlock_t *);
        switch (mode) {
          case Shared: {
            func = pthread_rwlock_rdlock;
            break;
          }
          case Exclusive: {
            func = pthread_rwlock_wrlock;
            break;
          }
        }
        IfNe0((*func)(&sharex.OsHandle));
        Sharex = &sharex;
      }

      /* Release our sharex, if any. */
      ~TLock() {
        assert(this);
        if (Sharex) {
          pthread_rwlock_unlock(&Sharex->OsHandle);
        }
      }

      /* Take over the donor lock's sharex, leaving the donor null.  If we're
         already holding a sharex, we release it.  If the donor lock is
         already null, then it stays null and we become null. */
      TLock &operator=(TLock &&that) noexcept {
        assert(this);
        assert(&that);
        this->~TLock();
        return *new (this) TLock(std::move(that));
      }

      /* True iff. we're currently locking a sharex. */
      operator bool() const noexcept {
        assert(this);
        return Sharex != nullptr;
      }

      /* Try to lock the given sharex in shared- or exclusive-mode and return
         success/failure.  We will not block.  If we succeed, any previous
         sharex we might have been locking will be freed in favor of the new
         one.  If we fail, our state remains unchanged. */
      bool Try(TSharex &sharex, TMode mode) {
        assert(this);
        assert(&sharex);
        bool success;
        int (*func)(pthread_rwlock_t *);
        switch (mode) {
          case Shared: {
            func = pthread_rwlock_tryrdlock;
            break;
          }
          case Exclusive: {
            func = pthread_rwlock_trywrlock;
            break;
          }
        }
        int err = (*func)(&sharex.OsHandle);
        switch (err) {
          case 0: {
            this->~TLock();
            Sharex = &sharex;
            success = true;
            break;
          }
          case EBUSY: {
            success = false;
            break;
          }
          default: {
            ThrowSystemError(err);
          }
        }
        return success;
      }

      /* Swap a pair of locks. */
      friend void swap(TLock &lhs, TLock &rhs) {
        assert(&lhs);
        assert(&rhs);
        using std::swap;
        swap(lhs.Sharex, rhs.Sharex);
      }

      private:

      /* The sharex we're locking, if any. */
      TSharex *Sharex;

    };  // Lock

    /* Initially not locked by anyone. */
    TSharex() {
      IfNe0(pthread_rwlock_init(&OsHandle, nullptr));
    }

    /* Don't destroy a sharex while it's locked. */
    ~TSharex() {
      assert(this);
      pthread_rwlock_destroy(&OsHandle);
    }

    private:

    /* A handle to the OS object we wrap. */
    pthread_rwlock_t OsHandle;

  };  // TSharex

}  // Base
