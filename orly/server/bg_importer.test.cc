/* <orly/server/bg_importer.test.cc>

   Unit test for <orly/server/bg_importer.h>.

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

#include <sstream>

#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>  // TODO: remove me

#include <base/event_counter.h>
#include <base/fd.h>
#include <base/tmp_dir_maker.h>
#include <test/kit.h>
#include <util/path.h>

using namespace std;
using namespace Base;
using namespace Orly::Server;
using namespace Util;

/* A test event-handler. */
class TTestImporter final
    : public TBgImporter::THandler {
  public:

  /* Watch the given top-level directory. */
  explicit TTestImporter(const string &path) {
    /* Start the importer. */
    BgImporter = new TBgImporter(this, path);
  }

  /* Stop watching. */
  ~TTestImporter() {
    assert(this);
    /* Stop the importer. */
    delete BgImporter;
  }

  /* Wait for the given number of import-mode-end events, then
     take the event description string. */
  string WaitAndTake(uint64_t min_count) {
    assert(this);
    for (uint64_t count = 0; count < min_count; count += Counter.Pop());
    return Strm.str();
  }

  private:

  /* Write a description of the event. */
  virtual void BeginBgImport() const noexcept override {
    assert(this);
    Strm << "(begin)";
  }

  /* Write a description of the event. */
  virtual void EndBgImport() const noexcept override {
    assert(this);
    Strm << "(end)";
    Counter.Push();
  }

  /* Write a description of the event. */
  virtual void BgImport(const string &path) const noexcept override {
    assert(this);
    Strm << "(import " << path << ")";
  }

  /* We write descriptions of events to this stream. */
  mutable ostringstream Strm;

  /* Counts the number of times we end import mode. */
  mutable Base::TEventCounter Counter;

  /* Our importer.  Never null. */
  TBgImporter *BgImporter;

};  // TTestImporter

static void CreateDir(const string &tmp_dir, const char *name) {
  IfLt0(mkdir(MakePath({ tmp_dir.c_str() }, { name }).c_str(), 0777));
}

static void CreateFile(
    const string &tmp_dir, const char *dir, const char *name) {
  string
      src = MakePath({ tmp_dir.c_str() }, { name }),
      dst = MakePath({ tmp_dir.c_str(), dir }, { name });
  TFd fd(creat(src.c_str(), 0777));
  fd.Reset();
  IfLt0(rename(src.c_str(), dst.c_str()));
}

static void DeleteDir(const string &tmp_dir, const char *name) {
  auto path = MakePath({ tmp_dir.c_str() }, { name });
  for (;;) {
    if (rmdir(path.c_str()) == 0) {
      break;
    }
    if (errno != ENOTEMPTY) {
      ThrowSystemError(errno);
    }
  }
}

FIXTURE(Typical) {
  /* TODO: remove this logging jazz */
  openlog("bg_importer.test", LOG_PERROR | LOG_PID, LOG_USER);
  setlogmask(LOG_UPTO(LOG_DEBUG));
  TTmpDirMaker tmp_dir_maker("/tmp/bg_importer_test");
  const string &tmp_dir = tmp_dir_maker.GetPath();
  TTestImporter bg_importer(tmp_dir);
  CreateDir(tmp_dir, "dir1");
  CreateDir(tmp_dir, "dir2");
  sleep(1);  // TODO: remove me
  CreateFile(tmp_dir, "dir1", "a.dat");
  CreateFile(tmp_dir, "dir1", "b.dat");
  CreateFile(tmp_dir, "dir2", "c.dat");
  DeleteDir(tmp_dir, "dir1");
  DeleteDir(tmp_dir, "dir2");
  EXPECT_EQ(
      bg_importer.WaitAndTake(1),
      "(begin)"
      "(import /tmp/bg_importer_test/dir1/a.dat)"
      "(import /tmp/bg_importer_test/dir1/b.dat)"
      "(import /tmp/bg_importer_test/dir2/c.dat)"
      "(end)");
}
