/* <orly/svr_tools/herd.cc>

   Implements <orly/svr_tools/herd.h>.

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

#include <cassert>
#include <exception>
#include <sstream>
#include <string>

#include <syslog.h>

#include <signal/masker.h>

using namespace std;
using namespace chrono;

using namespace Signal;

using namespace Orly;
using namespace SvrTools;

THerd::THerd(int signal_number)
    : SignalHandlerInstaller(signal_number),
      Mask(TSet::Exclude, { signal_number }) {}

THerd::~THerd() {
  assert(this);
  int signal_number = SignalHandlerInstaller.GetSignalNumber();
  unique_lock<mutex> lock(Mutex);
  while (!Threads.empty()) {
    /* Interrupt each thread's I/O. */
    for (const auto &item: Threads) {
      pthread_kill(item.second, signal_number);
    }
    /* Wait up to 100ms for all threads to exit. */
    auto deadline = system_clock::now() + milliseconds(100);
    while (!Threads.empty()) {
      ThreadHasExited.wait_until(lock, deadline);
    }
  }
}

thread::id THerd::Launch(const char *role, TMain &&main) {
  assert(this);
  assert(role);
  assert(main);
  unique_lock<mutex> lock(Mutex);
  Main = move(main);
  thread temp(&THerd::Wrapper, this, role);
  auto result = temp.get_id();
  Threads.insert(make_pair(result, temp.native_handle()));
  temp.detach();
  while (Main) {
    ThreadHasEntered.wait(lock);
  }
  return result;
}

void THerd::Wrapper(const char *role) {
  assert(this);
  assert(role);
  /* Mask out all signals except for the one the destructor will send us. */
  TMasker masker(*Mask);
  string stamp;
  /* Format a string with which to identify log entries from this thread. */ {
    ostringstream strm;
    strm << role << '_' << hex << this_thread::get_id();
    stamp = strm.str();
    role = nullptr;
  }
  TMain main;
  /* Move the closure of the main entry point into a local variable and
     let the foreground know we've entered. */ {
    unique_lock<mutex> lock(Mutex);
    main = move(Main);
    assert(main);
    ThreadHasEntered.notify_one();
  }
  /* Run the main entry point, logging any error. */
  syslog(LOG_INFO, "%s; entering", stamp.c_str());
  try {
    main(stamp.c_str());
  } catch (const exception &ex) {
    syslog(LOG_ERR, "%s; error; %s", stamp.c_str(), ex.what());
  }
  syslog(LOG_INFO, "%s; exiting", stamp.c_str());
  /* Remove this thread from the set of active threads and let the
     foreground know we've exited. */ {
    unique_lock<mutex> lock(Mutex);
    Threads.erase(this_thread::get_id());
    ThreadHasExited.notify_one();
  }
}
