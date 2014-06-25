/* <base/file_lock.h>

   An RAII wrapper around Linux flock.

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

#include <sys/file.h>

#include <base/class_traits.h>
#include <base/fd.h>
#include <util/error.h>

namespace Base {

  /* An RAII wrapper around Linux flock. */
  class TFileLock final {
    NO_COPY(TFileLock);
    public:

    /* Whether the lock is shared or exclusive. */
    enum TSharing { Shared = LOCK_SH, Exclusive = LOCK_EX };

    /* The default is an inactive lock. */
    TFileLock() noexcept {}

    /* Leave the donor inactive. */
    TFileLock(TFileLock &&that) noexcept
        : Fd(std::move(that.Fd)) {}

    /* Lock the given file. */
    TFileLock(int fd, TSharing sharing) {
      if (fd >= 0) {
        Util::IfLt0(flock(fd, sharing));
        Fd = TFd(fd);
      }
    }

    /* Lock the given file. */
    TFileLock(TFd &&fd, TSharing sharing) {
      assert(&fd);
      if (fd.IsOpen()) {
        Util::IfLt0(flock(fd, sharing));
        Fd = std::move(fd);
      }
    }

    /* If active, unlock; otherwise, do nothing. */
    ~TFileLock() {
      assert(this);
      Reset();
    }

    /* Leave the donor inactive. */
    TFileLock &operator=(TFileLock &&that) noexcept {
      assert(this);
      assert(&that);
      Reset();
      Fd = std::move(that.Fd);
      return *this;
    }

    /* The file we lock, if any. */
    const TFd &GetFd() const noexcept {
      assert(this);
      return Fd;
    }

    /* Resume the default-constructed state. */
    TFileLock &Reset() noexcept {
      assert(this);
      if (Fd.IsOpen()) {
        flock(Fd, LOCK_UN);
        Fd.Reset();
      }
      return *this;
    }

    /* Swap two locks. */
    friend void swap(TFileLock &lhs, TFileLock &rhs) noexcept {
      assert(&lhs);
      assert(&rhs);
      using std::swap;
      swap(lhs.Fd, rhs.Fd);
    }

    private:

    /* See accessor. */
    TFd Fd;

  };  // TFileLock

}  // Base
