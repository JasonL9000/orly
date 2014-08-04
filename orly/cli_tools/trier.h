/* <orly/cli_tools/trier.h>

   Tries a call to the server.

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

#pragma once

#include <string>
#include <map>
#include <vector>

#include <base/opt.h>
#include <base/thrower.h>
#include <base/uuid.h>
#include <orly/cli_tools/tool.h>

namespace Orly {

  namespace CliTools {

    /* Tries a call to the server. */
    class TTrier final
        : public TTool {
      public:

      /* Thrown when the a named arg won't parse. */
      DEFINE_ERROR(TBadNamedArg, std::invalid_argument, "bad named arg");

      /* Command-line argument parser. */
      class TCmd final
          : public TTool::TCmd {
        public:

        /* Our meta-type. */
        class TMeta final
            : public TTool::TCmd::TMeta {
          public:

          /* Registers our fields. */
          TMeta();

        };  // TTrier::TCmd::TMeta

        /* Sets defaults. */
        TCmd();

        /* The fully qualified name of the method you want to run. */
        std::string FqMethodName;

        /* The arguments to the method you want to call.  Each string is in
           name:arg form. */
        std::vector<std::string> NamedArgs;

      };  // TTrier::TCmd

      /* Construct from arguments. */
      explicit TTrier(const TCmd &cmd);

      private:

      /* See base class. */
      virtual void DescribeAction(std::ostream &strm) override;

      /* See base class. */
      virtual void TakeAction(std::ostream &strm) override;

      /* Write an id. If the id is unknown, write the word 'new'. */
      static void WriteOptId(
          std::ostream &strm, const Base::TOpt<Base::TUuid> &id);

      /* The name of the package containing the method you want to run. */
      std::vector<std::string> PackageName;

      /* The name of the method you want to run (without the package name). */
      std::string MethodName;

      /* The named arguments to pass to the method. */
      std::map<std::string, std::string> NamedArgs;

    };  // TTrier

  }  // CliTools

}  // Orly
