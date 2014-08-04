/* <orly/cli_tools/installer.h>

   Installs a package on the server.

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
#include <vector>

#include <base/class_traits.h>
#include <orly/cli_tools/tool.h>
#include <socket/address.h>

namespace Orly {

  namespace CliTools {

    /* Installs a package on the server. */
    class TInstaller final
        : public TTool {
      public:

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

        };  // TInstaller::TCmd::TMeta

        /* Sets defaults. */
        TCmd();

        /* The logical branch under the server's package root in which to
           install the package. */
        std::string Branch;

        /* The file system path to the source script. */
        std::string SrcPath;

      };  // TInstaller::TCmd

      /* Construct from arguments. */
      explicit TInstaller(const TCmd &cmd);

      private:

      /* See base class. */
      virtual void DescribeAction(std::ostream &strm) override;

      /* See base class. */
      virtual void TakeAction(std::ostream &strm) override;

      /* Initialized from the command-line parser. */
      Socket::TAddress ServerAddress;

      /* Initialized from the command-line parser. */
      std::string Branch, SrcPath;

      /* SrcPath sans directory. */
      std::string SrcName;

    };  // TInstaller

  }  // CliTools

}  // Orly
