/* <orly/server/bg_importer.h>

   Imports data files in the background.

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

#include <cassert>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <thread>

#include <base/class_traits.h>
#include <base/file_lock.h>
#include <base/latch.h>

namespace Orly {

  namespace Server {

    /* Imports data files in the background. */
    class TBgImporter {
      NO_COPY(TBgImporter);
      public:

      /* A file to be imported from.  This object keeps the file open (for
         read-only) and locked (exclusively) as long as the object persists.
         When the object is destroyed, it releases the file and deletes it
         from the file system. */
      class TFile final {
        NO_COPY(TFile);
        public:

        /* Delete our file as we go. */
        ~TFile();

        /* The descriptor of the file we hold open and locked. */
        const Base::TFd &GetFd() const noexcept {
          assert(this);
          return FileLock.GetFd();
        }

        /* The path to the file we hold open and locked. */
        const std::string &GetPath() const noexcept {
          assert(this);
          return Path;
        }

        /* Open and lock the given file and return a file object to keep it
           that way.  If the file isn't there, return null. */
        static std::unique_ptr<TFile> TryNew(std::string &&path);

        private:

        /* Called by TryNew(). */
        TFile(std::string &&path, Base::TFd &&fd);

        /* See accessor. */
        std::string Path;

        /* The lock we hold on the file.  The descriptor it holds keeps the
           file open for reading only. */
        Base::TFileLock FileLock;

      };  // TBgImporter::TFile

      /* You must call Stop() before you allow this destructor to be
         called. */
      virtual ~TBgImporter();

      /* Begin watching the given directory for sub-directories.  The handle
         */
      int AddWatch(std::string &&path);

      /* Stop watching watching the given directory for sub-directories. */
      void RemoveWatch(int wd);

      protected:

      /* TODO. */
      TBgImporter();

      /* TODO. */
      void Start();

      /* TODO. */
      void Stop();

      private:

      /* Called by the background thread when it finds a new file in a watched
         sub-directory. */
      void OnFile(const char *path);

      /* The entry point of the background thread. */
      void Main();

      /* TODO */
      Base::TLatch<std::string, void> Latch;

      /* The background thread which enters at Main(). */
      std::thread Thread;

    };  // TBgImporter

  }  // Server

}  // Orly
