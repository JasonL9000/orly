/* <orly/cli_tools/trier.test.cc>

   Implements <orly/cli_tools/trier.h>.

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

#include <orly/cli_tools/trier.h>

#include <fstream>
#include <sstream>

#include <base/class_traits.h>
#include <base/scheduler.h>
#include <base/tmp_dir_maker.h>
#include <orly/compiler.h>
#include <orly/protocol.h>
#include <orly/client/client.h>
#include <orly/server/server.h>
#include <orly/cli_tools/installer.h>
#include <test/kit.h>
#include <util/path.h>

using namespace std;
using namespace chrono;
using namespace placeholders;

using namespace Base;
using namespace Util;

using ::Socket::TAddress;
using ::Orly::Compiler::Compile;
using ::Orly::Sabot::State;
using ::Orly::Sabot::ToNative;
using ::Orly::Server::TServer;
using ::Orly::CliTools::TInstaller;
using ::Orly::CliTools::TTrier;

/* A finalized version of the command-line parser used by the Orly server. */
class TServerCmd final
    : public TServer::TCmd {};

/* A Orly client that doesn't do very much. */
class TTestClient final
    : public ::Orly::Client::TClient {
  public:

  /* Connect locally with a new session id and a fixed, short TTL. */
  TTestClient()
      : ::Orly::Client::TClient(TAddress(
          TAddress::IPv4Loopback, Orly::DefaultPortNumber),
          TOpt<TUuid>(),
          seconds(30)) {}

  private:

  /* See base class.  We do nothing here. */
  virtual void OnPovFailed(const Base::TUuid &/*repo_id*/) override {}

  /* See base class.  We do nothing here. */
  virtual void OnUpdateAccepted(
      const Base::TUuid &/*repo_id*/,
      const Base::TUuid &/*tracking_id*/) override {}

  /* See base class.  We do nothing here. */
  virtual void OnUpdateReplicated(
      const Base::TUuid &/*repo_id*/,
      const Base::TUuid &/*tracking_id*/) override {}

  /* See base class.  We do nothing here. */
  virtual void OnUpdateDurable(
      const Base::TUuid &/*repo_id*/,
      const Base::TUuid &/*tracking_id*/) override {}

  /* See base class.  We do nothing here. */
  virtual void OnUpdateSemiDurable(
      const Base::TUuid &/*repo_id*/,
      const Base::TUuid &/*tracking_id*/) override {}

};  // TTestClient

FIXTURE(Typical) {
  /* Make some temp directories to work in. */
  TTmpDirMaker
      root_dir("/tmp/trier.test"),
      src_dir ("/tmp/trier.test/src"),
      out_dir ("/tmp/trier.test/out"),
      pkg_dir ("/tmp/trier.test/pkg");
  /* Some names. */
  const string
      pkg_name = "install_me",
      src_name = pkg_name + ".orly",
      src_path = MakePath(
          true, { src_dir.GetPath().c_str() }, { src_name.c_str() }),
      out_path = MakePath(
          true, { out_dir.GetPath().c_str() }, { pkg_name.c_str(), ".1.so" });
  /* Start a server.  It will look for its packages in 'pkg'.
     NOTE: We start the server before running the compiler because the
     compiler needs a TypeCzar to exist.  The server makes one.  This is
     klunky, but whatevs. */
  TServerCmd server_cmd;
  server_cmd.InstanceName = "trier_test";
  server_cmd.StartingState = "SOLO";
  server_cmd.Create = true;
  server_cmd.MemorySim = true;
  server_cmd.MemorySimMB = 64;
  server_cmd.PageCacheSizeMB = 64;
  server_cmd.BlockCacheSizeMB = 64;
  server_cmd.UpdatePoolSize = 1000;
  server_cmd.UpdateEntryPoolSize = 1000;
  server_cmd.DiskBufferBlockPoolSize = 1000;
  server_cmd.PackageDirectory = pkg_dir.GetPath();
  TScheduler scheduler(TScheduler::TPolicy(64, 128, milliseconds(30000)));
  TServer server(&scheduler, server_cmd);
  /* Write out the package source in script. */ {
    ofstream strm(src_path);
    strm
        << "package #1;" << endl
        << "sum = (a + b) where { a = given::(int); b = given::(int); };"
        << endl;
  }
  /* Compile the package into a library.  The source in 'src' will be compiled
     to a .so in 'out'. */ {
    ostringstream err_strm;
    Compile(
        TPath(src_path), out_dir.GetPath(), true, false, false, err_strm);
    EXPECT_EQ(err_strm.str(), string());
  }
  EXPECT_TRUE(ExistsPath(out_path.c_str()));
  /* Run an installer.  The library in 'out' will be installed in 'pkg'. */ {
    TInstaller::TCmd cmd;
    cmd.SrcPath = src_path;
    TInstaller installer(cmd);
    ostringstream tmp_strm;
    installer(tmp_strm);
    EXPECT_EQ(tmp_strm.str(), string());
  }
  /* Make a client and use it to call the package we just installed, to make
     sure it's working. */ {
    TTrier::TCmd cmd;
    cmd.FqMethodName = "pkg/sum";
    cmd.NamedArgs = { "a:101", "b:202" };
    TTrier trier(cmd);
    ostringstream tmp_strm;
    trier(tmp_strm);
    EXPECT_EQ(tmp_strm.str(), string());
  }
  /* Shut the server down. */
  scheduler.Shutdown(milliseconds(0));
}
