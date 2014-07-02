/* <orly/server/bg_importer.exercise.cc>

   Runs a do-nothing background importer until SIGINT.

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

#include <orly/server/ws.h>

#include <syslog.h>

#include <orly/server/bg_importer.h>
#include <signal/handler_installer.h>
#include <signal/masker.h>
#include <signal/set.h>

using namespace std;
using namespace Orly::Server;
using namespace Signal;

/* A do-nothing event-handler. */
class TDoNothingImporter final
    : public TBgImporter::THandler {
  public:

  /* Watch the given top-level directory. */
  explicit TDoNothingImporter(const char *path) {
    /* Start the importer. */
    BgImporter = new TBgImporter(this, path);
  }

  /* Stop watching. */
  ~TDoNothingImporter() {
    assert(this);
    /* Stop the importer. */
    delete BgImporter;
  }

  private:

  /* Do-nothing. */
  virtual void BeginBgImport() const noexcept override {}

  /* Do-nothing. */
  virtual void EndBgImport() const noexcept override {}

  /* Do-nothing. */
  virtual void BgImport(const string & /*path*/) const noexcept override {}

  /* Our importer.  Never null. */
  TBgImporter *BgImporter;

};  // TDoNothingImporter

/* Very main.  So running.  Wow. */
int main(int argc, char *argv[]) {
  /* Start the log. */
  openlog("bg_importer.exercise", LOG_PERROR | LOG_PID, LOG_USER);
  setlogmask(LOG_UPTO(LOG_DEBUG));
  /* Handle SIGINT. */
  THandlerInstaller handle_sigint(SIGINT);
  TMasker mask_all_but_sigint(*TSet(TSet::Exclude, { SIGINT }));
  /* Make an el-fake-o importer. */
  TDoNothingImporter bg_importer((argc >= 2) ? argv[1] : "./");
  /* Wait to be told to stop. */
  pause();
  return EXIT_SUCCESS;
}
