/* <orly/cli_tools/tool.cc>

   Implements <orly/cli_tools/tool.h>.

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

#include <orly/cli_tools/tool.h>

#include <cassert>
#include <cstdlib>
#include <sstream>

#include <orly/protocol.h>
#include <util/error.h>

using namespace std;
using namespace chrono;

using namespace Base;
using namespace Socket;
using namespace Util;

using namespace Orly;
using namespace CliTools;

TTool::TCmd::TMeta::TMeta(const char *desc)
    : TLog::TCmd::TMeta(desc) {
  Param(
      &TCmd::ServerAddress, "server_address", Optional, "server_address\0sa\0",
      "The address where the Orly server can be found."
  );
  Param(
      &TCmd::SessionId, "session_id", Optional, "session_id\0sid\0",
      "The id of the session to use.  Ignored if you specify --new_session."
  );
  Param(
      &TCmd::PovId, "pov_id", Optional, "pov_id\0pid\0",
      "The id of the POV to use.  Ignored if you specify --new_pov."
  );
  Param(
      &TCmd::Verbose, "verbose", Optional, "verbose\0v\0",
      "Describe actions in detail."
  );
  Param(
      &TCmd::NoAction, "no_action", Optional, "no_action\0na\0",
      "Describe actions but don't take them.  Implies --verbose."
  );
  Param(
      &TCmd::NewSession, "new_session", Optional, "new_session\0nsid\0",
      "Create a new session.  The value of --session_id will be ignored."
  );
  Param(
      &TCmd::NewPov, "new_pov", Optional, "new_pov\0npid\0",
      "Create a new POV.  The value of --pov_id will be ignored."
  );
  Param(
      &TCmd::UpdateEnv, "update_env", Optional, "update_env\0ue\0",
      "Update the session id and pov id environment variables at exit."
  );
}

bool TTool::TCmd::CheckArgs(const TMeta::TMessageConsumer &cb) {
  assert(this);
  assert(cb);
  return
      TLog::TCmd::CheckArgs(cb)
      && SetId(SessionId, NewSession, SessionIdEnvVar, cb)
      && SetId(PovId, NewPov, PovIdEnvVar, cb);
}

TTool::TCmd::TCmd()
    : ServerAddress(TAddress::IPv4Loopback, DefaultPortNumber),
      Verbose(false), NoAction(false), NewSession(false), NewPov(false), UpdateEnv(false) {}

bool TTool::TCmd::SetId(TOpt<TUuid> &id, bool new_id, const char *env_var, const TMeta::TMessageConsumer &cb) {
  assert(&id);
  assert(env_var);
  assert(cb);
  bool success = (id || new_id);
  if (!success) {
    const char *env_val = getenv(env_var);
    if (env_val) {
      try {
        id = TUuid(env_val);
        success = true;
      } catch (const exception &ex) {
        ostringstream strm;
        strm << "could not parse " << env_var << ": " << ex.what();
        cb(strm.str());
      }
    } else {
      ostringstream strm;
      strm << "must set " << env_var << " or provide an explicit argument";
      cb(strm.str());
    }
  }
  return success;
}

void TTool::operator()(ostream &strm) {
  assert(this);
  assert(&strm);
  /* If we're being chatty, say up-front what we'd like to accomplish. */
  if (Verbose) {
    DescribeAction(strm);
  }
  /* If this is a no-action, then we're already done. */
  if (NoAction) {
    strm << "No action taken." << endl;
    return;
  }
  TakeAction(strm);
  if (UpdateEnv) {
    SetEnv(strm, SessionIdEnvVar, GetSessionId());
    SetEnv(strm, PovIdEnvVar, PovId);
  }
}

TUuid TTool::EstablishPov() {
  assert(this);
  if (!PovId) {
    PovId = **NewFastPrivatePov(TOpt<TUuid>());
  }
  return *PovId;
}

TTool::TTool(const TCmd &cmd)
    : Client::TClient(cmd.ServerAddress, cmd.SessionId, seconds(30)),
      Verbose(cmd.Verbose | cmd.NoAction), NoAction(cmd.NoAction), UpdateEnv(cmd.UpdateEnv), PovId(cmd.PovId) {}

void TTool::OnPovFailed(const Base::TUuid &/*repo_id*/) {}

void TTool::OnUpdateAccepted(const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TTool::OnUpdateReplicated(const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TTool::OnUpdateDurable(const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TTool::OnUpdateSemiDurable(const Base::TUuid &/*repo_id*/, const Base::TUuid &/*tracking_id*/) {}

void TTool::SetEnv(ostream &strm, const char *env_var, const TOpt<TUuid> &id) const {
  assert(this);
  assert(&strm);
  assert(env_var);
  assert(&id);
  if (id) {
    /* extra */ {
      ostringstream strm;
      strm << *id;
      IfLt0(setenv(env_var, strm.str().c_str(), true));
    }
    if (Verbose) {
      strm << env_var << " set." << endl;
    }
  } else {
    IfLt0(unsetenv(env_var));
    if (Verbose) {
      strm << env_var << " unset." << endl;
    }
  }
}

vector<string> TTool::SplitPackageName(const string &package_name) {
  assert(&package_name);
  /* Scan thru the package name, looking for slashes. */
  string::size_type
      start = 0,
      limit;
  vector<string> result;
  for (;;) {
    /* Find the next slash or the end of the string. */
    limit = package_name.find('/', start);
    auto name = package_name.substr(start, limit - start);
    /* If we're at the end and we have an empty name, it means we ended with a trailing slash
       (which we'll ignore) or the whole string was empty to start with.  Either way, we're done. */
    if (name.empty()) {
      if (limit != string::npos) {
        THROW_ERROR(TBadName) << '"' << package_name << "\" must not contain the empty string";
      }
      break;
    }
    /* Make sure the name is kosher and append it to the vector we're building. */
    ValidateName(name);
    result.push_back(move(name));
    /* If we're at the end, exit the loop; otherwise, continue with the next character past the slash. */
    if (limit == string::npos) {
      break;
    }
    start = limit + 1;
  }
  return move(result);
}

void TTool::ValidateName(const string &name) {
  assert(&name);
  assert(!name.empty());
  auto iter = name.begin();
  if (!isalpha(*iter) && *iter != '_') {
    THROW_ERROR(TBadName) << '"' << name << "\" must begin with a letter or underscore";
  }
  for (++iter; iter != name.end(); ++iter) {
    if (!isalnum(*iter) && *iter != '_') {
      THROW_ERROR(TBadName) << '"' << name << "\" must contain only letters, numbers, and underscores";
    }
  }
}

const char
    *TTool::SessionIdEnvVar = "STIG_SESSION_ID",
    *TTool::PovIdEnvVar     = "STIG_POV_ID";

