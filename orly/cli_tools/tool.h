/* <orly/cli_tools/tool.h>

   The base for all the command-line cli_tools in this directory.

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
#include <ostream>
#include <string>
#include <vector>

#include <base/class_traits.h>
#include <base/log.h>
#include <base/opt.h>
#include <base/thrower.h>
#include <base/uuid.h>
#include <orly/client/client.h>
#include <socket/address.h>

namespace Orly {

  namespace CliTools {

    /* The base for all the command-line cli_tools in this directory. */
    class TTool
        : public ::Orly::Client::TClient {
      public:

      /* The error thrown by ValidateName() when a name is bogus. */
      DEFINE_ERROR(TBadName, std::invalid_argument, "bad name");

      /* The command-line parser for the arguments the cli_tools have in common. */
      class TCmd
          : public ::Base::TLog::TCmd {
        public:

        /* We'll let this be public, so Main<> can call it for us. */
        using Base::TCmd::Parse;

        /* Our meta-type. */
        class TMeta
            : public Base::TLog::TCmd::TMeta {
          public:

          protected:

          /* Registers our fields. */
          TMeta(const char *desc);

        };  // TTool::TCmd::TMeta

        /* See base class.  We'll use this opportunity to check for environment variables. */
        virtual bool CheckArgs(const TMeta::TMessageConsumer &cb) override;

        /* The address where the Orly server can be found.
           Defaults to the local host and the usual Orly port number. */
        Socket::TAddress ServerAddress;

        /* The id of the session to use.
           The default is taken from the environment variable SessionIdEnvVar.
           Ignored if NewSession is true. */
        Base::TOpt<Base::TUuid> SessionId;

        /* The id of the POV to use.
           The default is taken from the environment variable PovIdEnvVar.
           Ignored if NewPov is true. */
        Base::TOpt<Base::TUuid> PovId;

        /* True if we want to tell the user what we're doing. */
        bool Verbose;

        /* True if we don't want to actually do work, just describe what we might do. */
        bool NoAction;

        /* Create a new session.  The value of SessionId will be ignored. */
        bool NewSession;

        /* Create a new POV.  The value of PovId will be ignored. */
        bool NewPov;

        /* Update the environment variables SessionIdEnvVar and PovIdEnvVar with the
           session id and pov id, respectively, at program exit. */
        bool UpdateEnv;

        protected:

        /* Sets defaults. */
        TCmd();

        private:

        /* Set via out-param an id (session or POV) based on its value as parsed from the
           command-line, its associated --new flag, and/or its enviroment variable.  If there is
           an error, call the error-reporting callback.  Return success/failure. */
        static bool SetId(Base::TOpt<Base::TUuid> &id, bool new_id, const char *env_var, const TMeta::TMessageConsumer &cb);

      };  // TTool::TCmd

      /* Do the work of the tool and, if appropriate, updates the environment variables.
         Send the user-visible output to the given stream. */
      void operator()(std::ostream &strm);

      /* Return via out-parameter the id of the POV requested at the command-line.
         If there was no set POV, start one now (as a child of the global POV) and return its id. */
      Base::TUuid EstablishPov();

      /* See the TCmd field of the same name. */
      const Base::TOpt<Base::TUuid> &GetPovId() const {
        assert(this);
        return PovId;
      }

      /* See the TCmd field of the same name. */
      bool IsNoAction() const {
        assert(this);
        return NoAction;
      }

      /* See the TCmd field of the same name. */
      bool IsVerbose() const {
        assert(this);
        return Verbose;
      }

      /* The names of the environment variables we use. */
      static const char
          *SessionIdEnvVar,
          *PovIdEnvVar;

      protected:

      /* Caches some fields from the command-line parser and becomes a Orly client with a new session. */
      explicit TTool(const TCmd &cmd);

      /* See base class. */
      virtual void OnPovFailed(const Base::TUuid &repo_id) override;

      /* See base class. */
      virtual void OnUpdateAccepted(const Base::TUuid &repo_id, const Base::TUuid &tracking_id) override;

      /* See base class. */
      virtual void OnUpdateReplicated(const Base::TUuid &repo_id, const Base::TUuid &tracking_id) override;

      /* See base class. */
      virtual void OnUpdateDurable(const Base::TUuid &repo_id, const Base::TUuid &tracking_id) override;

      /* See base class. */
      virtual void OnUpdateSemiDurable(const Base::TUuid &repo_id, const Base::TUuid &tracking_id) override;

      /* Override to describe the work of the tool. */
      virtual void DescribeAction(std::ostream &strm) = 0;

      /* Set an environment variable to the given id.
         If the id is unknown, unset the environment variable.
         If verbose, chat about the environment to the given stream. */
      void SetEnv(std::ostream &strm, const char *env_var, const Base::TOpt<Base::TUuid> &id) const;

      /* Override to do the work of the tool. */
      virtual void TakeAction(std::ostream &strm) = 0;

      /* Split the package name on slashes and return it as a vector of individual names.
         Also validate each name, throwing if badness is found. */
      static std::vector<std::string> SplitPackageName(const std::string &package_name);

      /* Throw if the name is bad. */
      static void ValidateName(const std::string &name);

      private:

      /* Initialized from the command-line parser. */
      bool Verbose, NoAction, UpdateEnv;

      /* Initialized from the command-line parser. */
      Base::TOpt<Base::TUuid> PovId;

    };  // TTool

  }  // CliTools

}  // Orly

