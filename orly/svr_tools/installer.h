/* <orly/svr_tools/installer.h>

   Serve the client's installer tool.

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

#include <linux/limits.h>
#include <netinet/in.h>

#include <base/class_traits.h>
#include <base/fd.h>
#include <base/log.h>
#include <base/thrower.h>
#include <socket/address.h>
#include <orly/client/client.h>
#include <orly/svr_tools/herd.h>

namespace Orly {

  namespace SvrTools {

    /* Serve the client's installer tool. */
    class TInstaller final {
      public:

      /* The default port number on which we listen. */
      static constexpr in_port_t DefaultPortNumber = 19382;

      /* The error thrown by ValidateName() when a name is bogus. */
      DEFINE_ERROR(TBadName, std::invalid_argument, "bad name");

      /* Command-line argument parser. */
      class TCmd final
          : public Base::TLog::TCmd {
        public:

        /* We'll let this be public, so Main<> can call it for us. */
        using Base::TCmd::Parse;

        /* Our meta-type. */
        class TMeta final
            : public Base::TLog::TCmd::TMeta {
          public:

          /* Registers our fields. */
          TMeta();

        };  // TInstaller::TCmd::TMeta

        /* Sets defaults. */
        TCmd();

        /* The port number on which we are listening for clients. */
        in_port_t PortNumber;

        /* The port number on which the Orly server is listening for\
           clients. */
        in_port_t SvrPortNumber;

        /* Directory into which to place source files. */
        std::string SrcDir;

        /* The file system path to the Orly server's package root. */
        std::string SvrPackageRoot;

      };  // TInstaller::TCmd

      /* Construct from arguments. */
      explicit TInstaller(const TCmd &cmd);

      /* Pauses, waiting for SIGINT, then returns. */
      void operator()(std::ostream &);

      /* Send a source script to the installation server. */
      static void Install(
          const Socket::TAddress &svr_address, const std::string &branch,
          const std::string &src_name, const std::string &src_text);

      private:

      /* Our Orly client. */
      class TClient final
          : public Client::TClient {
        public:

        /* Always uses a new session with a fixed, short ttl. */
        TClient(in_port_t svr_port_number);

        private:

        /* See base class. */
        virtual void OnPovFailed(const Base::TUuid &repo_id) override;

        /* See base class. */
        virtual void OnUpdateAccepted(
            const Base::TUuid &repo_id,
            const Base::TUuid &tracking_id) override;

        /* See base class. */
        virtual void OnUpdateReplicated(
            const Base::TUuid &repo_id,
            const Base::TUuid &tracking_id) override;

        /* See base class. */
        virtual void OnUpdateDurable(
            const Base::TUuid &repo_id,
            const Base::TUuid &tracking_id) override;

        /* See base class. */
        virtual void OnUpdateSemiDurable(
            const Base::TUuid &repo_id,
            const Base::TUuid &tracking_id) override;

      };  // TInstaller::TClient

      /* The header sent by the client to begin a call.
         It is followed immediately by the contents of the source file. */
      struct [[gnu::packed]] THeader final {

        /* The size of the source file, in bytes. */
        uint32_t SrcSize;

        /* The name of the source file, which must not contain any slashes.
           This cannot be the empty string. */
        char SrcName[PATH_MAX];

        /* The server namespace in which to install the package.
           This can be the empty string. */
        char Branch[PATH_MAX];

        /* The name of the user who sent this header.
           This can be the empty string. */
        char UserName[PATH_MAX];

      };  // TInstaller::THeader

      /* Accepts connections and launches workers to serve them. */
      void AcceptorMain(const char *stamp, Base::TFd &fd);

      /* Serves to a connected client. */
      void WorkerMain(const char *stamp, Base::TFd &fd);

      /* Split the branch on slashes and return it as a vector of individual
         names. */
      static std::vector<std::string> SplitBranch(const std::string &branch);

      /* Throw if the name is bad. */
      static void ValidateName(const std::string &name);

      /* Initialized from the command-line parser. */
      in_port_t SvrPortNumber;

      /* Initialized from the command-line parser. */
      std::string SvrPackageRoot;

      /* Initialized from the command-line parser. */
      std::string SrcDir;

      /* Manages our threads. */
      THerd Herd;

    };  // TInstaller

  }  // SvrTools

}  // Orly
