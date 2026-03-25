#include <gtest/gtest.h>
#include "JobSystem/JobSystem.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

TEST(JobSystemTest, SubmitAndCompleteVoidJobs) {
    JobSystem js(2);
    std::atomic<int> counter{ 0 };

    for (int i = 0; i < 10; ++i) {
        js.submitJob([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }

    js.waitForAll();
    EXPECT_EQ(counter.load(), 10);
}

TEST(JobSystemTest, SubmitJobsWithResult) {
    JobSystem js(2);
    auto future1 = js.submitJobWithResult([](int a, int b) { return a + b; }, 2, 3);
    auto future2 = js.submitJobWithResult([](std::string s) { return s + "Test"; }, std::string("Job"));

    EXPECT_EQ(future1.get(), 5);
    EXPECT_EQ(future2.get(), "JobTest");
}

TEST(JobSystemTest, ExceptionHandlingInJob) {
    JobSystem js(2);
    auto future = js.submitJobWithResult([]() -> int { throw std::runtime_error("fail"); });

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(JobSystemTest, ShutdownPreventsNewJobs) {
    JobSystem js(2);
    js.shutdown();
    std::atomic<int> counter{ 0 };
    js.submitJob([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    js.waitForAll();
    EXPECT_EQ(counter.load(), 0);
}

TEST(JobSystemTest, WaitForAllTimeout) {
    JobSystem js(2);
    std::atomic<int> counter{ 0 };
    js.submitJob([&counter]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        counter.fetch_add(1, std::memory_order_relaxed);
        });

    bool completed = js.waitForAll(std::chrono::milliseconds(50));
    EXPECT_FALSE(completed);
    js.waitForAll();
    EXPECT_EQ(counter.load(), 1);
}

TEST(JobSystemTest, GetThreadAndJobCounts) {
    JobSystem js(3);
    EXPECT_EQ(js.getThreadCount(), 3);
    EXPECT_EQ(js.getPendingJobCount(), 0);
    EXPECT_EQ(js.getActiveJobCount(), 0);

    js.submitJob([]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
    EXPECT_GE(js.getPendingJobCount(), 0);
    EXPECT_GE(js.getActiveJobCount(), 0);
    js.waitForAll();
}

TEST(JobSystemTest, MultipleJobsAndShutdown) {
    JobSystem js(4);
    std::atomic<int> counter{ 0 };
    for (int i = 0; i < 20; ++i) {
        js.submitJob([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    js.waitForAll();
    EXPECT_EQ(counter.load(), 20);
    js.shutdown();
    EXPECT_TRUE(js.isShuttingDown());
}