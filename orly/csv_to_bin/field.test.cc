/* <orly/csv_to_bin/field.test.cc>

   Unit test for <orly/csv_to_bin/field.h>.

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

#include <orly/csv_to_bin/field.h>

#include <test/kit.h>

using namespace std;
using namespace Base;
using namespace Orly::CsvToBin;

/* A structure to play with. */
class TFoo final {
  NO_COPY(TFoo);
  public:

  /* Initialize the fields to known values, so we can tell when we've
     changed them. */
  TFoo()
      : A(false), B(0), C(0), D(), E() {}

  /* Some fields to play with. */
  bool A;
  int B;
  double C;
  string D;
  TUuid E;
  //Chrono::TTimePnt F;

  /* Metadata describing our fields.  Expressing this as a static local of
     a static member function seems like a good pattern to follow.  It gets
     around static data segment initialization issues and it allows the
     constructors called here access to the private fields they're
     describing. */
  static const TFields<TFoo> &GetFields() {
    static const TFields<TFoo> fields {
      NEW_FIELD(TFoo, A),
      NEW_FIELD(TFoo, B),
      NEW_FIELD(TFoo, C),
      NEW_FIELD(TFoo, D),
      NEW_FIELD(TFoo, E)
    };
    return fields;
  }

};  // TFoo

FIXTURE(Typical) {
  const auto &fields = TFoo::GetFields();
  EXPECT_EQ(fields.GetSize(), 5u);
  TFoo foo;
  fields.SetVals(foo, TJson::TObject {
      { "A", true }, { "B", 101 }, { "C", 98.6 }, { "D", "hello"},
      { "E", "1b4e28ba-2fa1-11d2-883f-b9a761bde3fb" } });
  EXPECT_TRUE(foo.A);
  EXPECT_EQ(foo.B, 101);
  EXPECT_EQ(foo.C, 98.6);
  EXPECT_EQ(foo.D, "hello");
  EXPECT_EQ(foo.E, TUuid("1b4e28ba-2fa1-11d2-883f-b9a761bde3fb"));
}