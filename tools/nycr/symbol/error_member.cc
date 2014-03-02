/* <tools/nycr/symbol/error_member.cc> 

   Implements <tools/nycr/symbol/error_member.h>.

   Copyright 2010-2014 Tagged
   
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
   
     http://www.apache.org/licenses/LICENSE-2.0
   
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <tools/nycr/symbol/error_member.h>

#include <tools/nycr/symbol/write_xml.h>

using namespace std;
using namespace Tools::Nycr::Symbol;

TErrorMember::~TErrorMember() {
  assert(this);
  SetCompound(0);
}

const TName &TErrorMember::GetName() const {
  return Name;
}

const TKind *TErrorMember::TryGetKind() const {
  return 0;
}

void TErrorMember::WriteRhs(ostream &strm) const {
  assert(&strm);
  strm << TLower(Name);
}

void TErrorMember::WriteXml(ostream &strm) const {
  assert(&strm);
  strm << TXmlTag(TXmlTag::ErrorMember, Open)
       << TXml(Name)
       << TXmlTag(TXmlTag::ErrorMember, Close);
}

const TName TErrorMember::Name("error");
