/* <orly/cli_tools/trier.cc>

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

#include <cassert>
#include <sstream>

#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#include <base/fd.h>
#include <orly/atom/kit2.h>
#include <orly/atom/suprena.h>
#include <orly/closure.h>
#include <orly/client/program/parse_stmt.h>
#include <orly/client/program/translate_expr.h>
#include <orly/sabot/state_dumper.h>

using namespace std;
using namespace chrono;

using namespace Base;
using namespace Socket;

using namespace Orly;
using namespace Atom;
using namespace Client::Program;
using namespace Orly::CliTools;

TTrier::TCmd::TMeta::TMeta()
    : TTool::TCmd::TMeta(
          "Tries calling a method in an installed Orly package.") {
  Param(
      &TCmd::FqMethodName, "fq_method_name", Required,
      "The fully qualified name of the method you want to run.");
  Param(
      &TCmd::NamedArgs, "named_args", Optional,
      "The arguments to the method you want to run, in name:arg form.");
}

TTrier::TCmd::TCmd() {}

TTrier::TTrier(const TCmd &cmd)
    : TTool(cmd) {
  /* Split the fq-name into the path and the method name. */
  PackageName = SplitPackageName(cmd.FqMethodName);
  if (PackageName.empty()) {
    THROW_ERROR(TBadName) << "fully qualified method name is missing";
  }
  MethodName = move(PackageName.back());
  PackageName.pop_back();
  /* Map the named args. */
  for (const auto &named_arg: cmd.NamedArgs) {
    /* Split the named arg string into name and arg and make sure the name is
       kosher. */
    auto colon_pos = named_arg.find(':');
    if (colon_pos == string::npos || colon_pos == 0 ||
        colon_pos == named_arg.size() - 1) {
      THROW_ERROR(TBadNamedArg)
          << '"' << named_arg
          << "\" must separate name from value with a colon";
    }
    auto
        name = named_arg.substr(0, colon_pos),
        arg  = named_arg.substr(colon_pos + 1);
    /* Make sure the name is kosher, then file the arg away under that name.
       Blow up if the name is a dupe. */
    ValidateName(name);
    if (!NamedArgs.insert(make_pair(name, arg)).second) {
      THROW_ERROR(TBadNamedArg) << '"' << named_arg << "\" is not unique";
    }
  }
}

void TTrier::DescribeAction(ostream &strm) {
  assert(this);
  assert(&strm);
  strm
      << "To try:" << endl
      << "  SessuionId  = ";
  WriteOptId(strm, GetSessionId());
  strm
      << endl
      << "  PovId       = ";
  WriteOptId(strm, GetPovId());
  strm
      << endl
      << "  PackageName = [ ";
  bool sep = false;
  for (const auto &name: PackageName) {
    if (sep) {
      strm << ", ";
    } else {
      sep = true;
    }
    strm << '"' << name << '"';
  }
  strm
      << " ]" << endl
      << "  MethodName  = \"" << MethodName << '"' << endl
      << "  NamedArgs   = [ ";
  sep = false;
  for (const auto &item: NamedArgs) {
    if (sep) {
      strm << ", ";
    } else {
      sep = true;
    }
    strm << '"' << item.first << "\":\"" << item.second << '"';
  }
  strm << " ]" << endl;
}

void TTrier::TakeAction(ostream &strm) {
  assert(this);
  assert(&strm);
  /* We'll build a closure around the method name and the named args. */
  void *state_alloc = alloca(Sabot::State::GetMaxStateSize());
  TClosure closure(MethodName);
  /* Add the named args to the closure one at a time. */
  for (const auto &item: NamedArgs) {
    /* Construct an echo statement using the argument and parse it.  This is a
       little klunky, but... */
    ostringstream strm;
    strm << "echo " << item.second << ';';
    ParseStmtStr(
        strm.str().c_str(),
        [state_alloc, &closure, &item](const TStmt *stmt) {
          /* We know it's an echo statement because we just wrote it. */
          const auto *echo_stmt = dynamic_cast<const TEchoStmt *>(stmt);
          assert(echo_stmt);
          /* Add it to the closure by name. */
          Sabot::State::TAny::TWrapper state(
              NewStateSabot(echo_stmt->GetExpr(), state_alloc));
          closure.AddArgBySabot(item.first, state);
        }
    );
  }
  /* Make the call, wait for the result, and print it. */
  auto pov_id = EstablishPov();
  if (IsVerbose()) {
    strm
        << "SessionId = " << *GetSessionId() << endl
        << "PovId     = " << pov_id << endl;
  }
  auto result = Try(pov_id, PackageName, closure);
  Sabot::State::TAny::TWrapper(
      (*result)->GetValue().NewState(
          (*result)->GetArena().get(), state_alloc)
      )->Accept(Sabot::TStateDumper(strm));
  strm << endl;
}

void TTrier::WriteOptId(ostream &strm, const TOpt<TUuid> &id) {
  assert(&strm);
  assert(&id);
  if (id) {
    strm << *id;
  } else {
    strm << "new";
  }
}
