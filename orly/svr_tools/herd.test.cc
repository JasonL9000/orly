/* <orly/svr_tools/herd.test.cc>

   Unit test for <orly/svr_tools/herd.h>.

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

#include <orly/svr_tools/herd.h>

#include <atomic>

#include <unistd.h>

#include <test/kit.h>

using namespace std;
using namespace placeholders;

using namespace Orly::SvrTools;

FIXTURE(Stamp) {
  const string expected = "test";
  string actual;
  atomic_bool go(false);
  THerd herd;
  herd.Launch(
      expected.c_str(),
      [&actual, &go](const char *stamp) {
        actual = stamp;
        go = true;
      }
  );
  while (!go);
  EXPECT_EQ(actual.substr(0, expected.size()), expected);
}

FIXTURE(Interrupt) {
  const int expected = 101;
  int actual;
  /* extra */ {
    atomic_bool go(false);
    THerd herd;
    herd.Launch(
        "stubborn",
        [&expected, &actual, &go](const char *) {
          actual = expected;
          go = true;
          pause();
        }
    );
    while (!go);
  }
  EXPECT_EQ(actual, expected);
}
