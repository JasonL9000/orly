/* <base/fs_notifier.test.cc>

   Unit test for <base/fs_notifier.h>.

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

#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <base/event_semaphore.h>
#include <base/tmp_dir_maker.h>
#include <base/zero.h>
#include <test/kit.h>
#include <util/path.h>

using namespace std;
using namespace Base;
using namespace Util;

/* Builds a comma-separated string describing a sequence of events. */
class TBuilder final {
  NO_COPY(TBuilder);
  public:

  /* Start empty. */
  TBuilder()
      : Sep(false) {}

  /* Append the event to the string we're building. */
  void Push(const TFsNotifier::TEvent &event) {
    assert(this);
    if (Sep) {
      Strm << ", ";
    } else {
      Sep = true;
    }
    Strm << event;
  }

  /* Synthesize the given event and append it to the string we're building. */
  void Push(int wd, uint32_t mask, bool is_dir, const char *name) {
    assert(this);
    assert(name);
    union {
      TFsNotifier::TEvent event;
      char space[TFsNotifier::MinBufferSize];
    } tmp;
    Zero(tmp);
    tmp.event.wd = wd;
    tmp.event.mask = mask | (is_dir ? IN_ISDIR : 0);
    tmp.event.len = (strlen(name) + 4) & ~3;
    strcpy(tmp.event.name, name);
    Push(tmp.event);
  }

  /* Take the built string. */
  string Take() {
    assert(this);
    string result = Strm.str();
    Sep = false;
    return move(result);
  }

  private:

  /* We build to this stream. */
  ostringstream Strm;

  /* True when the next push needs a comma before it. */
  bool Sep;

};  // TBuilder

/* A temporary directory which will be watched by a watcher. */
class TWatchedDir final {
  NO_COPY(TWatchedDir);
  public:

  /* Construct the named directory under the system temp directory and add
     it to the notifier to be watched.  As we create and delete files and
     directories in this temp workspace, build the events the notifier
     should be seeing. */
  TWatchedDir(const char *name, TBuilder &builder, TFsNotifier &fs_notifier)
      : TmpDirMaker(MakePath({ "/tmp" }, { name })),
        Builder(builder), FsNotifier(fs_notifier) {
    assert(&fs_notifier);
    assert(&builder);
    Wd = fs_notifier.AddWatch(GetPath());
  }

  /* Remove the directory from the notifier and from the disk. */
  ~TWatchedDir() {
    assert(this);
    FsNotifier.RemoveWatch(Wd);
    Builder.Push(Wd, IN_IGNORED, false, "");
  }

  /* Create a directory in the temp space. */
  void CreateDir(const char *name) {
    assert(this);
    const char *path = MakeScratchPath(name);
    IfLt0(mkdir(path, 0777));
    Builder.Push(Wd, IN_CREATE, true, name);
  }

  /* Create a file in the temp space. */
  void CreateFile(const char *name) {
    assert(this);
    const char *path = MakeScratchPath(name);
    int fd;
    IfLt0(fd = creat(path, 0777));
    close(fd);
    Builder.Push(Wd, IN_CREATE, false, name);
    Builder.Push(Wd, IN_OPEN, false, name);
    Builder.Push(Wd, IN_CLOSE_WRITE, false, name);
  }

  /* Delete a directory from the temp space. */
  void DeleteDir(const char *name) {
    assert(this);
    const char *path = MakeScratchPath(name);
    IfLt0(rmdir(path));
    Builder.Push(Wd, IN_DELETE, true, name);
  }

  /* Delete a file from the temp space. */
  void DeleteFile(const char *name) {
    assert(this);
    const char *path = MakeScratchPath(name);
    IfLt0(unlink(path));
    Builder.Push(Wd, IN_DELETE, false, name);
  }

  /* The path to the temp directory being watched. */
  const char *GetPath() const {
    assert(this);
    return TmpDirMaker.GetPath().c_str();
  }

  private:

  /* Build the path to the named file, overwriting our scratch space. */
  const char *MakeScratchPath(const char *name) {
    assert(this);
    ScratchPath = MakePath({ GetPath() }, { name });
    return ScratchPath.c_str();
  }

  /* Creates and destroys the temp directory. */
  TTmpDirMaker TmpDirMaker;

  /* The builder to which we push our synthetic events. */
  TBuilder &Builder;

  /* The notifier we register with to be watched. */
  TFsNotifier &FsNotifier;

  /* The 'watch descriptor' returned by the notifier, uniquely identifying
     this directory among the things the notifier is watching. */
  int Wd;

  /* Used by MakeScratchPath() to build temp strings. */
  string ScratchPath;

};  // TWatchedDir

/* Creates a notifier and listens to it on a background thread. */
class TWatcher final {
  NO_COPY(TWatcher);
  public:

  /* Send events to the given builder using our background thread. */
  explicit TWatcher(TBuilder &builder)
      : FsNotifier(IN_NONBLOCK), Builder(builder) {
    assert(&builder);
    BgThread = thread(&TWatcher::BgMain, this);
  }

  /* Stop the background thread. */
  ~TWatcher() {
    assert(this);
    Stop.Push();
    BgThread.join();
  }

  /* The notifier we're listening to in the background.  You may call the
     AddWatch() and RemoveWatch() functions on this object from the
     foreground. */
  TFsNotifier FsNotifier;

  private:

  /* The entry point of the background thread. */
  void BgMain() {
    assert(this);
    pollfd events[2];
    events[0].fd = FsNotifier.GetFd();
    events[0].events = POLLIN;
    events[1].fd = Stop.GetFd();
    events[1].events = POLLIN;
    /* Loop here, sending events from the notifier to the builder.  Stop when
       there are no events pending and the semaphore is set. */
    do {
      IfLt0(poll(events, 2, -1));
      if (events[0].revents & POLLIN) {
        for (; FsNotifier; ++FsNotifier) {
          Builder.Push(*FsNotifier);
        }
      }
    } while ((events[1].revents & POLLIN) == 0);
  }

  /* The builder to which we push events. */
  TBuilder &Builder;

  /* Set when we want the background thread to stop. */
  TEventSemaphore Stop;

  /* The background thread. */
  thread BgThread;

};  // TWatcher

FIXTURE(Typical) {
  /* We will fill these builders with events: actual events from a watcher
     and expected events from watched directories. */
  TBuilder actual_builder, expected_builder;
  /* Start watching on a background thread. */
  unique_ptr<TWatcher> watcher(new TWatcher(actual_builder));
  /* Perform some file system operations for the watcher to see. */
  unique_ptr<TWatchedDir> dir1(new TWatchedDir(
      "fs_notifier_1", expected_builder, watcher->FsNotifier));
  dir1->CreateFile("hello.txt");
  dir1->CreateFile("doctor.txt");
  dir1->CreateFile("name.txt");
  dir1->CreateDir("songs");
  dir1->CreateDir("evil_secrets");
  dir1->DeleteFile("doctor.txt");
  dir1->DeleteDir("evil_secrets");
  unique_ptr<TWatchedDir> dir2(new TWatchedDir(
      "fs_notifier_2", expected_builder, watcher->FsNotifier));
  dir2->CreateFile("continue.txt");
  dir1.reset();
  dir2.reset();
  /* Stop the watcher. */
  watcher.reset();
  /* Compare the actual events to the expected events. */
  EXPECT_EQ(actual_builder.Take(), expected_builder.Take());
}
