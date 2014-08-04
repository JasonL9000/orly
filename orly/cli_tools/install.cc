/* <orly/cli_tools/install.cc>

   Install a package.

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

#include <orly/tool_main.h>
#include <orly/cli_tools/installer.h>

using namespace Orly;
using namespace CliTools;

int main(int argc, char *argv[]) {
  return Main<TInstaller>(argc, argv);
}
