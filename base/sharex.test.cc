/* <base/sharex.test.cc>

   Unit test for <base/sharex.h>.

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

#include <base/sharex.h>

#include <algorithm>
#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include <test/kit.h>

using namespace std;
using namespace Base;

/* Covers Elems, Rng, and Dist. */
TSharex Sharex;

/* A bunch of random numbers that should always sum to zero. */
vector<int> Elems;

/* Our source of chaos. */
mt19937 Rng((random_device())());

/* The numbers in Elems are in this distribution. */
uniform_int_distribution<> Dist(1, 100);

/* The number of workers ready to run. */
atomic_size_t ReadyCount;

/* Signals the workers to start. */
atomic_bool Go;

/* The worker is ready and waiting. */
static void Sync() {
  ++ReadyCount;
  while (!Go);
}

/* Repatedbly share-locks Elems and sums it, making sure the result is always
   zero. */
static void ReaderMain(int &sum) {
  Sync();
  for (int i = 0; sum = 0 && i < 100; ++i) {
    TSharex::TLock lock(Sharex, TSharex::Shared);
    for (int elem: Elems) {
      sum += elem;
    }
  }
}

/* Repeatedly exclusive-lcoks Elems and pushes in numbers is positive and
   negative pairs, such that the sum should always be zero.  Also shuffles
   Elems around, just for funsies. */
static void WriterMain() {
  //minstd_rand rng(random_device());
  Sync();
  for (int i = 0; i < 100; ++i) {
    TSharex::TLock lock(Sharex, TSharex::Exclusive);
    int x = Dist(Rng);
    Elems.push_back(x);
    Elems.push_back(-x);
    shuffle(Elems.begin(), Elems.end(), Rng);
  }
}

/* Runs a swam of readers and writers and returns true if Elems ends up non-
   empty but still sums to zero. */
static bool Swarm() {
  constexpr size_t
      reader_count = 20,
      writer_count = 20;
  /* Prepare the workers. */
  ReadyCount = 0;
  Go = false;
  vector<int> reader_sums(reader_count, 0);
  vector<thread> workers;
  workers.reserve(reader_count + writer_count);
  for (size_t i = 0; i < reader_count; ++i) {
    workers.push_back(thread(ReaderMain, ref(reader_sums[i])));
  }
  for (size_t i = 0; i < writer_count; ++i) {
    workers.push_back(thread(WriterMain));
  }
  /* Wait for them to sync, the let them go and wait for them to finish. */
  while (ReadyCount != workers.size());
  Go = true;
  for (auto &worker: workers) {
    worker.join();
  }
  workers.clear();
  /* Each reader has summed Elems and stopped early if the sum was ever
     non-zero.  If all is well, the sum of the sums should also be zero. */
  int sum = 0;
  for (int reader_sum: reader_sums) {
    sum += reader_sum;
  }
  return !Elems.empty() && sum == 0;
}

FIXTURE(Typical) {
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_TRUE(Swarm());
  }
}