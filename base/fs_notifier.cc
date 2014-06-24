/* <base/fs_notifier.cc>

   Implements <base/fs_notifier.h>.

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

#include <base/fs_notifier.h>

#include <algorithm>
#include <new>

#include <unistd.h>

#include <util/error.h>

using namespace std;
using namespace Base;
using namespace Util;

constexpr size_t
    TFsNotifier::MinBufferSize,
    TFsNotifier::DefBufferSize;

TFsNotifier::TFsNotifier(int flags, size_t buffer_size)
    : BufferSize(max(buffer_size, MinBufferSize)), Fd(inotify_init1(flags)) {
  Buffer = static_cast<char *>(malloc(buffer_size));
  if (!Buffer) {
    throw bad_alloc();
  }
  Cursor = Buffer;
  Limit = Buffer;
}

/* TODO */
TFsNotifier::~TFsNotifier() {
  assert(this);
  free(Buffer);
}

TFsNotifier &TFsNotifier::operator++() {
  assert(this);
  const TEvent &event = Peek();
  Cursor += sizeof(TEvent) + event.len;
  return *this;
}

int TFsNotifier::AddWatch(const char *path, uint32_t mask) {
  assert(this);
  return IfLt0(inotify_add_watch(Fd, path, mask));
}

void TFsNotifier::RemoveWatch(int wd) {
  assert(this);
  IfLt0(inotify_rm_watch(Fd, wd));
}

const TFsNotifier::TEvent *TFsNotifier::TryPeek() const {
  assert(this);
  if (Cursor >= Limit) {
    ssize_t size = read(Fd, Buffer, BufferSize);
    if (size < 0) {
      if (errno == EWOULDBLOCK) {
        size = 0;
      } else {
        ThrowSystemError(errno);
      }
    }
    Cursor = Buffer;
    Limit = Buffer + size;
  }
  return (Cursor < Limit)
      ? reinterpret_cast<inotify_event *>(Cursor) : nullptr;
}

ostream &operator<<(ostream &strm, const inotify_event &event) {
  assert(&strm);
  assert(&event);
  if (event.mask & IN_Q_OVERFLOW) {
    /* The event queue overflowed since it was last read. */
    strm << "IN_Q_OVERFLOW";
  } else {
    strm << event.wd << ' ';
    if (event.mask & IN_UNMOUNT) {
      /* File system containing watched file or directory was unmounted. */
      strm << "IN_UNMOUNT";
    } else if (event.mask & IN_IGNORED) {
      /* Watch was explicitly removed from the notifier (via RemoveWatch), or
         the watched file or directory was deleted. */
      strm << "IN_IGNORED";
    } else {
      switch (event.mask & IN_ALL_EVENTS) {
        /* The watched file or directory was read from. */
        case IN_ACCESS: {
          strm << "IN_ACCESS ";
          break;
        }
        /* The watched file or directory's metadata was changed. */
        case IN_ATTRIB: {
          strm << "IN_ATTRIB ";
          break;
        }
        /* The watched file or directory was closed
           (and was not open for writing). */
        case IN_CLOSE_NOWRITE: {
          strm << "IN_CLOSE_NOWRITE ";
          break;
        }
        /* The watched file or directory was closed
           (and was open for writing). */
        case IN_CLOSE_WRITE: {
          strm << "IN_CLOSE_WRITE ";
          break;
        }
        /* A file or directory was created in the watched directory. */
        case IN_CREATE: {
          strm << "IN_CREATE ";
          break;
        }
        /* A file or directory was deleted in the watched directory. */
        case IN_DELETE: {
          strm << "IN_DELETE ";
          break;
        }
        /* The watched file or directory was deleted. */
        case IN_DELETE_SELF: {
          strm << "IN_DELETE_SELF ";
          break;
        }
        /* The watched file or directory was modified. */
        case IN_MODIFY: {
          strm << "IN_MODIFY ";
          break;
        }
        /* The watched file or directory was moved. */
        case IN_MOVE_SELF: {
          strm << "IN_MOVE_SELF ";
          break;
        }
        /* A file or directory was moved away from the watched directory. */
        case IN_MOVED_FROM: {
          strm << "IN_MOVED_FROM (cookie: "<< event.cookie << ") ";
          break;
        }
        /* A file or directory was moved into the watched directory. */
        case IN_MOVED_TO: {
          strm << "IN_MOVED_TO (cookie: "<< event.cookie << ") ";
          break;
        }
        /* The watched file or directory was opened. */
        case IN_OPEN: {
          strm << "IN_OPEN ";
          break;
        }
        default: {
          strm << "<unknown event> ";
        }
      }
      strm << ((event.mask & IN_ISDIR) ? "dir " : "file ") << event.name;
    }
  }
  return strm;
}
