/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <errno.h>
#include <pthread.h>
#include <unistd.h>

TEST(pthread, pthread_key_create) {
  pthread_key_t key;
  ASSERT_EQ(0, pthread_key_create(&key, NULL));
  ASSERT_EQ(0, pthread_key_delete(key));
  // Can't delete a key that's already been deleted.
  ASSERT_EQ(EINVAL, pthread_key_delete(key));
}

static void* IdFn(void* arg) {
  return arg;
}

static void* SleepFn(void* arg) {
  sleep(reinterpret_cast<unsigned int>(arg));
  return NULL;
}

static void* SpinFn(void* arg) {
  volatile bool* b = reinterpret_cast<volatile bool*>(arg);
  while (!*b) {
  }
  return NULL;
}

static void* JoinFn(void* arg) {
  return reinterpret_cast<void*>(pthread_join(reinterpret_cast<pthread_t>(arg), NULL));
}

static void AssertDetached(pthread_t t, bool is_detached) {
  pthread_attr_t attr;
  ASSERT_EQ(0, pthread_getattr_np(t, &attr));
  int detach_state;
  ASSERT_EQ(0, pthread_attr_getdetachstate(&attr, &detach_state));
  pthread_attr_destroy(&attr);
  ASSERT_EQ(is_detached, (detach_state == PTHREAD_CREATE_DETACHED));
}

TEST(pthread, pthread_create) {
  void* expected_result = reinterpret_cast<void*>(123);
  // Can we create a thread?
  pthread_t t;
  ASSERT_EQ(0, pthread_create(&t, NULL, IdFn, expected_result));
  // If we join, do we get the expected value back?
  void* result;
  ASSERT_EQ(0, pthread_join(t, &result));
  ASSERT_EQ(expected_result, result);
}

TEST(pthread, pthread_no_join_after_detach) {
  pthread_t t1;
  ASSERT_EQ(0, pthread_create(&t1, NULL, SleepFn, reinterpret_cast<void*>(5)));

  // After a pthread_detach...
  ASSERT_EQ(0, pthread_detach(t1));
  AssertDetached(t1, true);

  // ...pthread_join should fail.
  void* result;
  ASSERT_EQ(EINVAL, pthread_join(t1, &result));
}

TEST(pthread, pthread_no_op_detach_after_join) {
  bool done = false;

  pthread_t t1;
  ASSERT_EQ(0, pthread_create(&t1, NULL, SpinFn, &done));

  // If thread 2 is already waiting to join thread 1...
  pthread_t t2;
  ASSERT_EQ(0, pthread_create(&t2, NULL, JoinFn, reinterpret_cast<void*>(t1)));

  sleep(1); // (Give t2 a chance to call pthread_join.)

  // ...a call to pthread_detach on thread 1 will "succeed" (silently fail)...
  ASSERT_EQ(0, pthread_detach(t1));
  AssertDetached(t1, false);

  done = true;

  // ...but t2's join on t1 still goes ahead (which we can tell because our join on t2 finishes).
  void* join_result;
  ASSERT_EQ(0, pthread_join(t2, &join_result));
  ASSERT_EQ(0, reinterpret_cast<int>(join_result));
}

TEST(pthread, pthread_join_self) {
  void* result;
  ASSERT_EQ(EDEADLK, pthread_join(pthread_self(), &result));
}

#if __BIONIC__ // For some reason, gtest on bionic can cope with this but gtest on glibc can't.

static void TestBug37410() {
  pthread_t t1;
  ASSERT_EQ(0, pthread_create(&t1, NULL, JoinFn, reinterpret_cast<void*>(pthread_self())));
  pthread_exit(NULL);
}

// We have to say "DeathTest" here so gtest knows to run this test (which exits)
// in its own process.
TEST(pthread_DeathTest, pthread_bug_37410) {
  // http://code.google.com/p/android/issues/detail?id=37410
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_EXIT(TestBug37410(), ::testing::ExitedWithCode(0), "");
}
#endif
