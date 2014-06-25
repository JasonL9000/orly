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

#include <cassert>
#include <exception>
#include <map>
#include <sstream>

#include <fcntl.h>
#include <poll.h>
#include <syslog.h>
#include <unistd.h>

#include <util/error.h>
#include <util/path.h>

using namespace std;
using namespace Orly::Server;
using namespace Base;
using namespace Util;

TBgImporter::TBgImporter(const THandler *handler, const char *top_path)
    : Handler(handler), TopPath(top_path), KeepGoing(true),
      FsNotifier(IN_NONBLOCK) {
  assert(handler);
  TopWd = FsNotifier.AddWatch(top_path, IN_CREATE);
  Watcher = thread(&TBgImporter::Watch, this);
}

TBgImporter::~TBgImporter() {
  assert(this);
  KeepGoing = false;
  Ping.Push();
  Watcher.join();
}

void TBgImporter::Watch() {
  assert(this);
  syslog(LOG_INFO, "bg_importer; started");
  pollfd events[2];
  events[0].fd = Ping.GetFd();
  events[0].events = POLLIN;
  events[1].fd = FsNotifier.GetFd();
  events[1].events = POLLIN;
  map<int, string> sub_dirs;
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
        syslog(LOG_INFO, "bg_importer; importing [%s]", path.c_str());
        Handler->BgImport(path);
        syslog(LOG_INFO, "bg_importer; unlinking [%s]", path.c_str());
        IfLt0(unlink(path.c_str()));
        continue;
      }
      syslog(LOG_WARNING, "bg_import; spurious event");
    }  // for
  }  // while
  syslog(LOG_INFO, "bg_importer; stopped");
}
