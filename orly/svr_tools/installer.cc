/* <orly/svr_tools/installer.cc>

   Implements <orly/svr_tools/installer.h>.

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

#include <cassert>
#include <sstream>

#include <fcntl.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#include <base/tmp_dir_maker.h>
#include <base/zero.h>
#include <io/endian.h>
#include <signal/handler_installer.h>
#include <signal/set.h>
#include <socket/address.h>
#include <orly/compiler.h>
#include <orly/protocol.h>
#include <orly/type/type_czar.h>
#include <util/error.h>
#include <util/path.h>
#include <util/io.h>

using namespace std;
using namespace chrono;
using namespace placeholders;

using namespace Base;
using namespace Io;
using namespace Signal;
using namespace Socket;
using namespace Util;

using namespace Orly;
using namespace SvrTools;

/* This is seriously ugly!
   It's necessary to run the Orly compiler, but it REALLY needs to go. */
bool PrintCmds = false;

TInstaller::TCmd::TMeta::TMeta()
    : TLog::TCmd::TMeta("Serve the client's install tool.") {
  Param(
      &TCmd::PortNumber, "port_number", Optional, "port_number\0pn\0",
      "The port on which we listen for connections."
  );
  Param(
      &TCmd::SvrPortNumber, "svr_port_number", Optional, "svr_port_number\0spn\0",
      "The port number on which the Orly server listens for connections."
  );
  Param(
      &TCmd::SrcDir, "src_dir", Optional, "src_dir\0sd\0",
      "The directory in which to place source files."
  );
  Param(
      &TCmd::SvrPackageRoot, "svr_package_root", Required,
      "The directory in which the Orly server looks for packages."
  );
}

TInstaller::TCmd::TCmd()
    : PortNumber(TInstaller::DefaultPortNumber),
      SvrPortNumber(Orly::DefaultPortNumber),
      SrcDir("/tmp/orly_installer") {}

TInstaller::TInstaller(const TCmd &cmd)
    : SvrPortNumber(cmd.SvrPortNumber), SvrPackageRoot(cmd.SvrPackageRoot),
      SrcDir(cmd.SrcDir) {
  /* Open up. */
  TAddress address(TAddress::IPv4Any, cmd.PortNumber);
  TFd fd(socket(address.GetFamily(), SOCK_STREAM, 0));
  /* Feel free to reuse the address after it closes. */
  int flag = true;
  IfLt0(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)));
  /* Bind and listen.  The connection backlog is being set arbitrarily here,
    but it shouldn't be an issue. */
  Bind(fd, address);
  IfLt0(listen(fd, 10));
  /* Launch the acceptor thread. */
  Herd.Launch(
      "acceptor", bind(&TInstaller::AcceptorMain, this, _1, move(fd)));
}

void TInstaller::operator()(ostream &) {
  assert(this);
  /* Intern types for the compiler. */
  const Type::TTypeCzar TypeCzar;
  /* Wait for the axe to fall. */
  int signal_number = SIGINT;
  THandlerInstaller signal_handler_installer(signal_number);
  sigsuspend(TSet(TSet::Exclude, { signal_number }).Get());
}

void TInstaller::Install(
    const TAddress &svr_address, const string &branch, const string &src_name,
    const string &src_text) {
  /* Initialize a header structure. */
  THeader header;
  Zero(header);
  header.SrcSize = SwapEnds(static_cast<uint32_t>(src_text.size()));
  strncpy(header.SrcName, src_name.c_str(), sizeof(header.SrcName));
  header.SrcName[sizeof(header.SrcName) - 1] = '\0';
  strncpy(
      header.UserName, getpwuid(getuid())->pw_name, sizeof(header.UserName));
  header.Branch[sizeof(header.Branch) - 1] = '\0';
  strncpy(header.Branch, branch.c_str(), sizeof(header.UserName));
  header.UserName[sizeof(header.UserName) - 1] = '\0';
  /* Connect to the installation server. */
  TFd fd(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  Connect(fd, svr_address);
  /* Send the header and the source text. */
  WriteExactly(fd, &header, sizeof(header));
  WriteExactly(fd, src_text.data(), src_text.size());
  /* Get the reply. */
  uint32_t size;
  ReadExactly(fd, &size, sizeof(size));
  size = SwapEnds(size);
  string reply(size, '.');
  ReadExactly(fd, const_cast<char *>(reply.data()), size);
  /* If the reply is anything but the empty string, it's an error message. */
  if (!reply.empty()) {
    throw runtime_error(reply.c_str());
  }
}

TInstaller::TClient::TClient(in_port_t svr_port_number)
    : Client::TClient(TAddress(TAddress::IPv4Loopback, svr_port_number),
      TOpt<TUuid>(), seconds(30)) {}

void TInstaller::TClient::OnPovFailed(const Base::TUuid &/*repo_id*/) {}

void TInstaller::TClient::OnUpdateAccepted(
    const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TInstaller::TClient::OnUpdateReplicated(
    const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TInstaller::TClient::OnUpdateDurable(
    const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TInstaller::TClient::OnUpdateSemiDurable(
    const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TInstaller::AcceptorMain(const char *stamp, TFd &fd) {
  assert(this);
  assert(stamp);
  assert(&fd);
  assert(fd.IsOpen());
  /* Loop, accepting connections. */
  for (;;) {
    /* Wait for a connection. */
    TAddress address;
    TFd new_fd = Accept(fd, address);
    /* Handle the connection in a new thread. */
    auto thread_id = Herd.Launch(
        "worker", bind(&TInstaller::WorkerMain, this, _1, move(new_fd)));
    /* Log the event. */
    ostringstream strm;
    strm
        << "accepted connection from " << address
        << "; started worker " << hex << thread_id;
    syslog(LOG_INFO, "%s", strm.str().c_str());
  }
}

void TInstaller::WorkerMain(const char *stamp, TFd &fd) {
  assert(this);
  assert(stamp);
  assert(&fd);
  assert(fd.IsOpen());
  string reply;
  try {
    /* Try to read a header from the client. */
    THeader header;
    Zero(header);
    if (!TryReadExactly(fd, &header, sizeof(header))) {
      syslog(LOG_INFO, "%s; client hung up without making a request", stamp);
      return;
    }
    /* Get the size of the source file into host byte order and make sure the
       strings are null-terminated. */
    header.SrcSize = SwapEnds(header.SrcSize);
    header.SrcName[sizeof(header.SrcName) - 1] = '\0';
    header.Branch[sizeof(header.Branch) - 1] = '\0';
    header.UserName[sizeof(header.UserName) - 1] = '\0';
    syslog(
        LOG_INFO, "%s; rec'd header; user = \"%s\", src = \"%s\", size = %d, branch = \"%s\"",
        stamp, header.UserName, header.SrcName, header.SrcSize, header.Branch
    );
    /* Split the branch into component names. */
    auto fq_package_name = SplitBranch(header.Branch);
    /* Make a temp directory in which to work and compose the path to the source file we'll create. */
    TTmpDirMaker tmp_dir_maker(MakePath({ SrcDir.c_str(), stamp }, {}));
    string src_path = MakePath({ tmp_dir_maker.GetPath().c_str() }, { header.SrcName });
    /* Copy the source script from the socket to the source file. */ {
      TFd src(creat(src_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));
      syslog(LOG_INFO, "%s; created \"%s\"", stamp, src_path.c_str());
      char buf[4906];
      size_t size = header.SrcSize;
      while (size) {
        size_t actl = ReadAtMost(fd, buf, min(size, sizeof(buf)));
        WriteExactly(src, buf, actl);
        size -= actl;
        syslog(LOG_INFO, "%s; copied %ld byte(s) to src dir", stamp, actl);
      }
      syslog(LOG_INFO, "%s; copy to src dir complete", stamp);
    }
    #if 0
    syslog(LOG_INFO, "%s; compiling \"%s\"", stamp, src_path.c_str());
    string package_name;
    uint64_t version_number;
    /* Compile the package into a library. */ {
      auto versioned_name = Compiler::Compile(
          Jhm::TAbsPath(Jhm::TAbsBase(tmp_dir_maker.GetPath()), Jhm::TRelPath(header.SrcName)),
          tmp_dir_maker.GetPath(),
          false, false, false
      );
      package_name = versioned_name.Name.AsStr();
      version_number = versioned_name.Version;
    }
    /* Complete the fully qualified package name. */
    ValidateName(package_name);
    fq_package_name.push_back(package_name);
    /* Compose the name of the library we just compiled and the path to where it currently is. */
    string
        lib_name = ConcatCStrList({ package_name.c_str(), ".", to_string(version_number).c_str(), ".so" }),
        lib_path = MakePath({ tmp_dir_maker.GetPath().c_str() }, { lib_name.c_str() });
    syslog(LOG_INFO, "%s; produced library \"%s\", version %ld", stamp, lib_path.c_str(), version_number);
    /* Copy the library to the Orly server's package directory. */ {
      string dir_path = MakePath({ SvrPackageRoot.c_str(), header.Branch }, {});
      syslog(LOG_INFO, "%s; ensuring package branch exists \"%s\"", stamp, dir_path.c_str());
      EnsureDirExists(dir_path.c_str());
      string dest_path = MakePath({ dir_path.c_str() }, { lib_name.c_str() });
      syslog(LOG_INFO, "%s; opening source \"%s\"", stamp, lib_path.c_str());
      TFd src(open(lib_path.c_str(), O_RDONLY));
      struct stat st;
      IfLt0(fstat(src, &st));
      size_t size = st.st_size;
      /* Open (or create) the destination file and copy the source to it entirely.
         The destination file's permissions will be 'rwxrwxr-x'. */
      syslog(LOG_INFO, "%s; opening destination \"%s\"", stamp, dest_path.c_str());
      TFd dest(creat(dest_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));
      while (size) {
        ssize_t actl;
        IfLt0(actl = sendfile(dest, src, nullptr, size));
        syslog(LOG_INFO, "%s; copied %ld byte(s) to pkg dir", stamp, actl);
        size -= actl;
      }
      syslog(LOG_INFO, "%s; copy to pkg dir complete", stamp);
    }
    /* Connect to Orly, ask it to install the package, and wait for the
       ack. */
    syslog(LOG_INFO, "%s; calling InstallPackage", stamp);
    auto client = make_shared<TClient>(SvrPortNumber);
    auto ack = client->InstallPackage(fq_package_name, version_number);
    syslog(LOG_INFO, "%s; waiting for ack", stamp);
    ack->Sync();
    #endif
  } catch (const exception &ex) {
    /* Catch any error and reply with the error's text. */
    syslog(LOG_INFO, "%s; error; %s", stamp, ex.what());
    reply = ex.what();
  }
  /* Write the reply string and hang up. */
  uint32_t size = SwapEnds(static_cast<uint32_t>(reply.size()));
  WriteExactly(fd, &size, sizeof(size));
  WriteExactly(fd, reply.data(), reply.size());
}

vector<string> TInstaller::SplitBranch(const string &branch) {
  assert(&branch);
  /* Scan thru the branch, looking for slashes. */
  string::size_type
      start = 0,
      limit;
  vector<string> result;
  for (;;) {
    /* Find the next slash or the end of the string. */
    limit = branch.find('/', start);
    auto name = branch.substr(start, limit - start);
    /* If the name isn't empty, validate it and append it to the vector we're
       building. */
    if (!name.empty()) {
      ValidateName(name);
      result.push_back(move(name));
    }
    /* If we're at the end, exit the loop; otherwise, continue with the next
       character past the slash. */
    if (limit == string::npos) {
      break;
    }
    start = limit + 1;
  }
  return move(result);
}

void TInstaller::ValidateName(const string &name) {
  assert(&name);
  if (name.empty()) {
    THROW_ERROR(TBadName) << "must not be empty";
  }
  auto iter = name.begin();
  if (!isalpha(*iter) && *iter != '_') {
    THROW_ERROR(TBadName)
        << '"' << name << "\" must begin with a letter or underscore";
  }
  for (++iter; iter != name.end(); ++iter) {
    if (!isalnum(*iter) && *iter != '_') {
      THROW_ERROR(TBadName)
          << '"' << name
          << "\" must contain only letters, numbers, and underscores";
    }
  }
}
