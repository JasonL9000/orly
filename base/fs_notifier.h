/* <base/fs_notifier.h>

   An RAII wrapper around Linux inotify.

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
#include <cstdint>
#include <ostream>

#include <limits.h>
#include <sys/inotify.h>

#include <base/class_traits.h>
#include <base/fd.h>

namespace Base {

  /* An RAII wrapper around Linux inotify. */
  class TFsNotifier final {
    NO_COPY(TFsNotifier);
    public:

    /* Convenience. */
    using TEvent = inotify_event;

    /* The minimum and default sizes for the event buffer. */
    static constexpr size_t
        MinBufferSize = sizeof(TEvent) + NAME_MAX + 1,
        DefBufferSize = 0x10000;

    /* Flags may include IN_NONBLOCK and IN_CLOEXEC.  If the buffer size is
       less than the minimum, the minimum will be used. */
    explicit TFsNotifier(int flags = 0, size_t buffer_size = DefBufferSize);

    /* All watches are removed and any unhandled events are discarded. */
    ~TFsNotifier();

    /* If there events in the buffer, this returns true immediately;
       otherwise, behavior depends on whether the notifier is blocking or non-
       blocking.  If blocking, this function will block until there is at
       least one event in the buffer, then return true.  If non-blocking, this
       function will not block and will return false. */
    explicit operator bool() const {
      assert(this);
      return TryPeek() != nullptr;
    }

    /* The current event.  See Peek(). */
    const TEvent &operator*() const {
      assert(this);
      return Peek();
    }

    /* The current event.  See Peek(). */
    const TEvent *operator->() const {
      assert(this);
      return &Peek();
    }

    /* Advance to the next event. */
    TFsNotifier &operator++();

    /* Add a new watch or modify an existing one. */
    int AddWatch(const char *path, uint32_t mask = IN_ALL_EVENTS);

    /* The handle to the OS object.  Use this to wait for events. */
    const TFd &GetFd() const noexcept {
      assert(this);
      return Fd;
    }

    /* The current event.  If there are no events in the buffer, we'll wait
       for some.  If this is a non-blocking notifier, it is a logic error to
       call this function when there are no buffered events. */
    const TEvent &Peek() const {
      assert(this);
      auto *event = TryPeek();
      assert(event);
      return *event;
    }

    /* Remove an existing watch. */
    void RemoveWatch(int wd);

    /* The current event.  If there are no events in the buffer, we'll wait
       for some.  However, if this is a non-blocking notifier, we won't wait,
       we'll just return null. */
    const TEvent *TryPeek() const;

    private:

    /* The number of bytes pointed to by Buffer. */
    const size_t BufferSize;

    /* See accessor. */
    TFd Fd;

    /* A buffer into which we read events.  Never null.  Allocated during
       construction and never changed.  Freed on destruction. */
    char *Buffer;

    /* Pointers to the current event and past the last event.  As events are
       variable-sized structured, we use chars here.  When Cursor < Limit,
       we have events in the buffer.  Cursor always >= Buffer, Limit always
       <= Buffer + BufferSize. */
    mutable char *Cursor, *Limit;

  };  // TFsNotifier

}  // Base

/* A std inserter for inotify events.  This creates a human-readable
   description of the event.  It's useful for test, debugging, and logging. */
std::ostream &operator<<(std::ostream &strm, const inotify_event &event);
