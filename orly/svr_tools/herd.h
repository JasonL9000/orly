/* <orly/svr_tools/herd.h>

   A herd of threads with which to run a small service.

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

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

#include <signal.h>

#include <base/class_traits.h>
#include <signal/handler_installer.h>
#include <signal/set.h>

namespace Orly {

  namespace SvrTools {

    /* A herd of threads with which to run a small service. */
    class THerd final {
      public:

      /* TODO */
      using TMain = std::function<void (const char *stamp)>;

      /* TODO */
      explicit THerd(int signal_number = SIGUSR1);

      /* TODO */
      ~THerd();

      /* TODO */
      std::thread::id Launch(const char *role, TMain &&main);

      private:

      /* TODO */
      void Wrapper(const char *role);

      /* TODO */
      const Signal::THandlerInstaller SignalHandlerInstaller;

      /* Masks out all but the signal we use to interrupt our threads. */
      const Signal::TSet Mask;

      /* TODO */
      std::mutex Mutex;

      /* TODO */
      std::condition_variable ThreadHasEntered, ThreadHasExited;

      /* TODO */
      std::map<std::thread::id, pthread_t> Threads;

      /* TODO */
      TMain Main;

    };  // THerd

  }  // SvrTools

}  // Orly
