/* <orly/server/bg_importer.cc>

   Implements <orly/server/bg_importer.h>.

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

#include <orly/server/bg_importer.h>

#include <exception>

#include <fcntl.h>
#include <poll.h>
#include <syslog.h>
#include <unistd.h>

#include <base/dir_iter.h>
#include <base/fs_notifier.h>
#include <util/error.h>
#include <util/path.h>

using namespace std;
using namespace Orly::Server;
using namespace Base;
using namespace Util;

TBgImporter::TFile::~TFile() {
  assert(this);
  FileLock.Reset();
  unlink(Path.c_str());
}

unique_ptr<TBgImporter::TFile> TBgImporter::TFile::TryNew(string &&path) {
  assert(&path);
  unique_ptr<TBgImporter::TFile> result;
  int fd = open(path.c_str(), O_RDONLY);
  if (fd >= 0) {
    result.reset(new TFile(move(path), TFd(fd)));
  }
  return result;
}

TBgImporter::TFile::TFile(string &&path, TFd &&fd)
    : Path(move(path)), FileLock(move(fd), TFileLock::Exclusive) {}

#if 0
TBgImporter::TBgImporter(const THandler *handler, const string &top_path)
    : Handler(handler), TopPath(top_path), KeepGoing(true) {
  assert(handler);
  Watcher = thread(&TBgImporter::Watch, this);
}

TBgImporter::~TBgImporter() {
  assert(this);
  KeepGoing = false;
  StayAwake.Push();
  Watcher.join();
}

void TBgImporter::OnFile(const char *path) {
  assert(this);
  assert(path);
}

void TBgImporter::Watch() {
  assert(this);
  syslog(LOG_INFO, "bg_importer; started");
  try {
    /* Make a file system notifier and have it watch our top-level directory
       for the creation of sub-dirs.  As we find sub-dirs, we'll watch them,
       too. */
    TFsNotifier fs_notifier(IN_NONBLOCK);
    int top_wd = FsNotifier.AddWatch(TopPath.c_str(), IN_CREATE);
    map<int, string> sub_dirs;
    /* Make a poller so we can watch for a shutdown or for a file system
       event at the same time. */
    pollfd events[2];
    events[0].fd = StayAwake.GetFd();
    events[0].events = POLLIN;
    events[1].fd = FsNotifier.GetFd();
    events[1].events = POLLIN;
    /* Loop until shutdown, */
    while (KeepGoing) {
      /* Wait here for events. */
      syslog(LOG_INFO, "bg_importer; waiting");
      events[1].revents = 0;
      IfLt0(poll(events, 2, -1));
      if ((events[1].revents & POLLIN) == 0) {
        continue;
      }
      /* Handle all the events from the file system watcher. */
      for (; KeepGoing && FsNotifier; ++FsNotifier) {
        const auto &event = *FsNotifier;
        /* Was a sub-dir created? */
        if (event.wd == TopWd && (event.mask & IN_ISDIR) != 0 &&
            (event.mask & IN_CREATE) != 0) {
          /* Try to start watching it.  If we can't, then it must have come
             and gone before we could react, which also means it couldn't
             have had any files for us to import. */
          auto path = MakePath({ TopPath.c_str() }, { event.name });
          int wd;
          if (!fs_notifier.TryAddWatch(
              path.c_str(), wd, IN_IGNORED | IN_MOVED_TO)) {
            continue;
          }
          /* If this was the first sub-dir, notify our handler to enter
             import mode. */
          if (sub_dirs.empty()) {
            syslog(LOG_INFO, "bg_importer; begin import");
            Handler->BeginBgImport();
          }
          syslog(LOG_INFO, "bg_importer; started watching %s", path.c_str());
          sub_dirs[wd] = path;
          /* Scan the newly found directory for any files that might have
             gotten into it before we started watching. */
          for (TDirIter dir_iter(path.c_str()); dir_iter; ++dir_iter) {
            if (dir_iter.GetKind() == TDirIter::File) {

            }
          }
          continue;
        }
        /* If this event is not about a sub-dir, skip it. */
        auto iter = sub_dirs.find(event.wd);
        if (iter == sub_dirs.end()) {
          continue;
        }
        /* Was the sub-dir removed? */
        if ((event.mask & IN_IGNORED) != 0) {
          sub_dirs.erase(iter);
          syslog(
              LOG_INFO, "bg_importer; stopped watching [%s]",
              iter->second.c_str());
          /* If this was the last sub-dir, end importing. */
          if (sub_dirs.empty()) {
            syslog(LOG_INFO, "bg_importer; end import");
            Handler->EndBgImport();
          }
          continue;
        }
        /* Was a file moved into the sub-dir? */
        if ((event.mask & IN_ISDIR) == 0 && (event.mask & IN_MOVED_TO) != 0) {
          auto path = MakePath({ iter->second.c_str() }, { event.name });

          // TODO: fstat the file to make sure it exists

          syslog(LOG_INFO, "bg_importer; importing [%s]", path.c_str());
          Handler->BgImport(path);
          syslog(LOG_INFO, "bg_importer; unlinking [%s]", path.c_str());
          IfLt0(unlink(path.c_str()));
          continue;
        }
      }  // for

    }  // while

  } catch (const exception &ex) {
    syslog(LOG_INFO, "bg_importer; exception; %s", ex.what());
  }
  syslog(LOG_INFO, "bg_importer; stopped");



  /* Loop until the foreground tells us to stop. */
  while (KeepGoing) {
    /* Wait here for events. */
    syslog(LOG_INFO, "bg_importer; waiting");
    events[1].revents = 0;
    IfLt0(poll(events, 2, -1));
    if ((events[1].revents & POLLIN) == 0) {
      continue;
    }
    /* Handle all the events from the file system watcher. */
    for (; KeepGoing && FsNotifier; ++FsNotifier) {
      const auto &event = *FsNotifier;
      /* Was a sub-dir created? */
      if (event.wd == TopWd && (event.mask & IN_ISDIR) != 0 &&
          (event.mask & IN_CREATE) != 0) {
        /* Start watching it. */
        auto path = MakePath({ TopPath.c_str() }, { event.name });
        int wd = FsNotifier.AddWatch(
            path.c_str(), IN_IGNORED | IN_MOVED_TO);
        sub_dirs[wd] = path;

        /* If this was the first sub-dir, begin importing. */
        if (sub_dirs.size() == 1) {
          syslog(LOG_INFO, "bg_importer; begin import");
          Handler->BeginBgImport();
        }
        syslog(LOG_INFO, "bg_importer; started watching [%s]", path.c_str());
        for (TDirIter dir_iter(path.c_str()); dir_iter; ++dir_iter) {
          if (dir_iter.GetKind() == TDirIter::File) {

          }
        }
        continue;
      }
      /* If this event is not about a sub-dir, skip it. */
      auto iter = sub_dirs.find(event.wd);
      if (iter == sub_dirs.end()) {
        continue;
      }
      /* Was the sub-dir removed? */
      if ((event.mask & IN_IGNORED) != 0) {
        sub_dirs.erase(iter);
        syslog(
            LOG_INFO, "bg_importer; stopped watching [%s]",
            iter->second.c_str());
        /* If this was the last sub-dir, end importing. */
        if (sub_dirs.empty()) {
          syslog(LOG_INFO, "bg_importer; end import");
          Handler->EndBgImport();
        }
        continue;
      }
      /* Was a file moved into the sub-dir? */
      if ((event.mask & IN_ISDIR) == 0 && (event.mask & IN_MOVED_TO) != 0) {
        auto path = MakePath({ iter->second.c_str() }, { event.name });

        // TODO: fstat the file to make sure it exists

        syslog(LOG_INFO, "bg_importer; importing [%s]", path.c_str());
        Handler->BgImport(path);
        syslog(LOG_INFO, "bg_importer; unlinking [%s]", path.c_str());
        IfLt0(unlink(path.c_str()));
        continue;
      }
    }  // for
  }  // while
  syslog(LOG_INFO, "bg_importer; stopped");
}
#endif
