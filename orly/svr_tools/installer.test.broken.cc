/* <orly/svr_tools/installer.test.cc>

   Unit test for <orly/svr_tools/installer.h>.

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

#include <orly/svr_tools/installer.h>

#include <base/tmp_dir_maker.h>
#include <base/scheduler.h>
#include <orly/server/server.h>
#include <orly/cli_tools/trier.h>
#include <test/kit.h>

using namespace std;
using namespace chrono;

using namespace Base;
using namespace Socket;

using ::Orly::Server::TServer;
using ::Orly::SvrTools::TInstaller;
using ::Orly::CliTools::TTrier;

/* A finalized version of the command-line parser used by the Orly server. */
class TServerCmd final
    : public TServer::TCmd {};

FIXTURE(Typical) {
  /* Make a temp directories in which to store packages. */
  TTmpDirMaker pkg_dir("/tmp/installer_svr_test/pkg");
  /* Start a server. */
  TServerCmd server_cmd;
  server_cmd.InstanceName = "installer_svr_test";
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
  /* Uncomment these lines to turn on visible logging.
  server_cmd.All  = true;
  server_cmd.Echo = true;
  TLog log(server_cmd);
  */
  TScheduler scheduler(TScheduler::TPolicy(64, 128, milliseconds(30000)));
  TServer server(&scheduler, server_cmd);
  /* Start an installer mini-service and use it to install a package. */ {
    TInstaller::TCmd installer_cmd;
    installer_cmd.SvrPackageRoot = pkg_dir.GetPath();
    TInstaller installer(installer_cmd);
    TInstaller::Install(
        TAddress(TAddress::IPv4Loopback, TInstaller::DefaultPortNumber),
        "", "my_package.orly",
        "package #1; sum = (a + b) where { a = given::(int); b = given::(int); };"
    );
  }
  /* Use a trier object call our package. */ {
    TTrier::TCmd trier_cmd;
    trier_cmd.FqMethodName = "my_package/sum";
    trier_cmd.NamedArgs = { "a:101", "b:202" };
    TTrier trier(trier_cmd);
    ostringstream strm;
    trier(strm);
    EXPECT_EQ(strm.str(), string("303\n"));
  }
  /* Shut the server down. */
  scheduler.Shutdown(milliseconds(0));
}
