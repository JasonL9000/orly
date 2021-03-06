/* <mpl/extend.h>

   TODO

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

#include <base/identity.h>
#include <mpl/type_list.h>

namespace Mpl {

  /* Extend. */
  template <typename TLhs, typename TRhs>
  struct Extend;

  template <typename... TLhsElems, typename... TRhsElems>
  struct Extend<TTypeList<TLhsElems...>, TTypeList<TRhsElems...>>
      : public Base::identity<TTypeList<TLhsElems..., TRhsElems...>> {};

  template <typename TLhs, typename TRhs>
  using TExtend = typename Extend<TLhs, TRhs>::type;

}  // Mpl
