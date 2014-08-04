/* <orly/main.h>

   A templatized 'main' to be used by tools.

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

#include <cstdlib>
#include <exception>
#include <iostream>

#include <syslog.h>

#include <base/log.h>

namespace Orly {

  /* A templatized 'main' used by tools.  It assumes that TSomeTool has a
     command-line parser called TSomeTool::TCmd which inherits from
     ::Base::TLog::TCmd. */
  template <typename TSomeTool>
  int Main(int argc, char *argv[]) {
    /* Parse the command-line args and start the log. */
    typename TSomeTool::TCmd cmd;
    cmd.Parse(argc, argv, typename TSomeTool::TCmd::TMeta());
    Base::TLog log(cmd);
    /* Capture runtime errors in this try-catch. */
    int result;
    try {
      /* Construct and run the tool, sending its output to the console. */
      TSomeTool tool(cmd);
      tool(std::cout);
      result = EXIT_SUCCESS;
    } catch (const std::exception &ex) {
      /* Log the error and, if we're not echoing the log, report the error
         to stderr. */
      const char *msg = ex.what();
      syslog(LOG_ERR, "error: %s", msg);
      if (!cmd.Echo) {
        std::cerr << "error: " << msg << std::endl;
      }
      result = EXIT_FAILURE;
    }
    return result;
  }

}  // Orly
