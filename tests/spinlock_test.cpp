/*
 * basic_test.cpp
 */
#include <atomic>
#include <mutex>
#include "gtest/gtest.h"
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "yamc_testutil.hpp"


#define TEST_THREADS   8
#define TEST_ITERATION 10000u


using NormalMutexTypes = ::testing::Types<
  yamc::spin::mutex,
  yamc::spin_weak::mutex,
  yamc::spin_ttas::mutex
>;

template <typename Mutex>
struct NormalMutexTest : ::testing::Test {};

TYPED_TEST_CASE(NormalMutexTest, NormalMutexTypes);

// mutex::lock()
TYPED_TEST(NormalMutexTest, BasicLock)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        std::lock_guard<TypeParam> lk(mtx);
        counter = counter + 1;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// mutex::try_lock()
TYPED_TEST(NormalMutexTest, TryLock)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        while (!mtx.try_lock()) {
          std::this_thread::yield();
        }
        std::lock_guard<TypeParam> lk(mtx, std::adopt_lock);
        counter = counter + 1;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// mutex::try_lock() failure
TYPED_TEST(NormalMutexTest, TryLockFail)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  yamc::test::join_thread thd([&]{
    EXPECT_NO_THROW(mtx.lock());
    step.await();  // b1
    step.await();  // b2
    EXPECT_NO_THROW(mtx.unlock());
  });
  {
    step.await();  // b1
    EXPECT_FALSE(mtx.try_lock());
    step.await();  // b2
  }
}


// lockfree property of atomic<int>
TEST(AtomicTest, Lockfree)
{
  // std::atomic<int> type is always lock-free
  ASSERT_EQ(2, ATOMIC_INT_LOCK_FREE);
  // std::atomic<int> is lock-free
  std::atomic<int> i;
  ASSERT_TRUE(i.is_lock_free());
}


// backoff::exponential<100>
TEST(BackoffTest, Exponential100)
{
  using BackoffPolicy = yamc::backoff::exponential<100>;
  BackoffPolicy::state state;
  ASSERT_EQ(100u, state.initcount);
  ASSERT_EQ(100u, state.counter);
  for (int i = 0; i < 100; ++i) {
    BackoffPolicy::wait(state);  // wait 100
  }
  ASSERT_EQ(0u, state.counter);
  for (int i = 0; i < 2000; ++i) {
    BackoffPolicy::wait(state);
  }
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(0u, state.counter);
  BackoffPolicy::wait(state);
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(0u, state.counter);
}

// backoff::exponential<1>
TEST(BackoffTest, Exponential1)
{
  using BackoffPolicy = yamc::backoff::exponential<1>;
  BackoffPolicy::state state;
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(1u, state.counter);
  BackoffPolicy::wait(state);
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(0u, state.counter);
}
