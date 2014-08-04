/* <orly/cli_tools/installer.cc>

   Implements <orly/cli_tools/installer.h>.

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

#include <orly/cli_tools/installer.h>

#include <cassert>

#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>

#include <base/fd.h>
#include <orly/svr_tools/installer.h>
#include <util/io.h>

using namespace std;
using namespace chrono;

using namespace Base;
using namespace Socket;
using namespace Util;

using namespace Orly;
using namespace CliTools;

using TInstallSvr = SvrTools::TInstaller;

TInstaller::TCmd::TMeta::TMeta()
    : TTool::TCmd::TMeta("Installs a package on a Orly server.") {
  Param(
      &TCmd::Branch, "branch", Optional, "branch\0b\0",
      "The branch under the root in which to install the package."
  );
  Param(
      &TCmd::SrcPath, "src_path", Required,
      "The file system path to the source script."
  );
}

TInstaller::TCmd::TCmd() {
  ServerAddress = TAddress(
      TAddress::IPv4Loopback, TInstallSvr::DefaultPortNumber);
}

TInstaller::TInstaller(const TCmd &cmd)
    : TTool(cmd), ServerAddress(cmd.ServerAddress), Branch(cmd.Branch),
      SrcPath(cmd.SrcPath) {
  assert(&cmd);
  /* The source name is the string following the last slash in the source
     path. If the source path doesn't have any slashes, then the source name
     is the same as the source path. */
  auto start = SrcPath.rfind('/');
  SrcName = SrcPath.substr((start != string::npos) ? (start + 1) : 0);
}

void TInstaller::DescribeAction(ostream &strm) {
  assert(this);
  assert(&strm);
  strm
      << "To install:" << endl
      << "  ServerAddress = " << ServerAddress << endl
      << "  SrcPath       = \"" << SrcPath << '"' << endl
      << "  Branch        = \"" << Branch << '"' << endl
      << "  SrcName       = \"" << SrcName << '"' << endl;
}

void TInstaller::TakeAction(ostream &strm) {
  assert(this);
  assert(&strm);
  string src_text;
  /* Load the source text. */ {
    syslog(LOG_INFO, "opening source \"%s\"", SrcPath.c_str());
    TFd src(open(SrcPath.c_str(), O_RDONLY));
    struct stat st;
    IfLt0(fstat(src, &st));
    size_t size = st.st_size;
    syslog(LOG_INFO, "size = %ld byte(s)", size);
    src_text = string(size, '.');
    char *csr = const_cast<char *>(src_text.data());
    while (size) {
      size_t actl = ReadAtMost(src, csr, size);
      csr += actl;
      size -= actl;
      syslog(LOG_INFO, "read %ld byte(s)", actl);
    }
  }
  /* Install it. */
  TInstallSvr::Install(ServerAddress, Branch, SrcName, src_text);
  if (IsVerbose()) {
    strm << "Package installed." << endl;
  }
}
