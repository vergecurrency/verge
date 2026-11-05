// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <logging.h>
#include <scheduler.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
size_t Scheduled(CScheduler& scheduler)
{
    boost::chrono::system_clock::time_point first, last;
    return scheduler.getQueueInfo(first, last);
}

std::string ReadLog(const fs::path& path)
{
    std::ifstream input(path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(memory_usage_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(startup_log_buffer_is_bounded_and_reports_loss)
{
    const fs::path path = SetDataDir("bounded-log") / "debug.log";
    BCLog::Logger logger;
    logger.m_print_to_file = true;
    logger.m_log_timestamps = false;
    logger.m_file_path = path;
    const std::string message(2048, 'x');
    for (size_t i = 0; i < 20000; ++i) logger.LogPrintStr(message);
    auto stats = logger.GetBufferStats();
    BOOST_CHECK_LE(stats.bytes, BCLog::Logger::MAX_BUFFERED_LOG_BYTES);
    BOOST_CHECK_LE(stats.messages, BCLog::Logger::MAX_BUFFERED_LOG_MESSAGES);
    BOOST_CHECK_GT(stats.dropped, 0U);

    // Byte limits alone do not bound empty-message list nodes.
    for (size_t i = 0; i < 20000; ++i) logger.LogPrintStr("");
    stats = logger.GetBufferStats();
    BOOST_CHECK_LE(stats.messages, BCLog::Logger::MAX_BUFFERED_LOG_MESSAGES);
    const uint64_t dropped = stats.dropped;
    logger.LogPrintStr(std::string(BCLog::Logger::MAX_BUFFERED_LOG_BYTES + 1, 'x'));
    BOOST_CHECK_EQUAL(logger.GetBufferStats().dropped, dropped + 1);
    logger.LogPrintStr("last startup message\n");
    BOOST_REQUIRE(logger.OpenDebugLog());
    stats = logger.GetBufferStats();
    BOOST_CHECK_EQUAL(stats.bytes, 0U);
    BOOST_CHECK_EQUAL(stats.messages, 0U);
    BOOST_CHECK_EQUAL(stats.dropped, 0U);
    const auto contents = ReadLog(path);
    BOOST_CHECK(contents.find("log messages before opening") != std::string::npos);
    BOOST_CHECK(contents.find("last startup message") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(failed_log_reopen_keeps_stream_without_buffering)
{
    const fs::path directory = SetDataDir("reopen-log");
    const fs::path original = directory / "original.log";
    const fs::path replacement = directory / "missing" / "debug.log";
    BCLog::Logger logger;
    logger.m_print_to_file = true;
    logger.m_log_timestamps = false;
    logger.m_file_path = original;
    BOOST_REQUIRE(logger.OpenDebugLog());
    logger.m_file_path = replacement;
    logger.m_reopen_file = true;
    for (size_t i = 0; i < 10000; ++i) logger.LogPrintStr(std::string(256, 'x'));
    logger.LogPrintStr("kept old stream\n");
    BOOST_CHECK_EQUAL(logger.GetBufferStats().bytes, 0U);
    BOOST_CHECK_EQUAL(logger.GetBufferStats().messages, 0U);
    BOOST_CHECK(ReadLog(original).find("kept old stream") != std::string::npos);
    fs::create_directories(replacement.parent_path());
    logger.m_reopen_file = true;
    logger.LogPrintStr("replacement stream\n");
    BOOST_CHECK(ReadLog(replacement).find("replacement stream") != std::string::npos);
    BOOST_CHECK(ReadLog(original).find("replacement stream") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(callback_burst_uses_one_scheduler_job_and_keeps_order)
{
    CScheduler scheduler;
    SingleThreadedSchedulerClient client(&scheduler);
    size_t next = 0;
    constexpr size_t count = 50000;
    for (size_t i = 0; i < count; ++i) {
        client.AddToProcessQueue([&, i] { BOOST_CHECK_EQUAL(next++, i); });
    }
    BOOST_REQUIRE_EQUAL(Scheduled(scheduler), 1U);
    scheduler.stop(true);
    scheduler.serviceQueue();
    BOOST_CHECK_EQUAL(next, count);
    BOOST_CHECK_EQUAL(client.CallbacksPending(), 0U);
    BOOST_CHECK_EQUAL(Scheduled(scheduler), 0U);
}

BOOST_AUTO_TEST_CASE(manual_drain_does_not_schedule_a_job_per_callback)
{
    CScheduler scheduler;
    SingleThreadedSchedulerClient client(&scheduler);
    size_t completed = 0;
    for (size_t i = 0; i < 50000; ++i) client.AddToProcessQueue([&] { ++completed; });
    client.EmptyQueue();
    BOOST_CHECK_EQUAL(completed, 50000U);
    BOOST_CHECK_EQUAL(Scheduled(scheduler), 1U);
    BOOST_CHECK_EQUAL(client.CallbacksPending(), 0U);
    scheduler.stop(true);
    scheduler.serviceQueue();
    BOOST_CHECK_EQUAL(Scheduled(scheduler), 0U);
}

BOOST_AUTO_TEST_CASE(callback_exception_does_not_stall_remaining_callbacks)
{
    CScheduler scheduler;
    SingleThreadedSchedulerClient client(&scheduler);
    bool completed = false;
    client.AddToProcessQueue([] { throw std::runtime_error("test callback"); });
    client.AddToProcessQueue([&] { completed = true; });
    BOOST_CHECK_THROW(client.EmptyQueue(), std::runtime_error);
    client.EmptyQueue();
    BOOST_CHECK(completed);
    BOOST_CHECK_EQUAL(client.CallbacksPending(), 0U);
}

BOOST_AUTO_TEST_CASE(concurrent_producers_preserve_order_and_serial_execution)
{
    CScheduler scheduler;
    SingleThreadedSchedulerClient client(&scheduler);
    std::atomic<size_t> completed{0}, active{0}, errors{0};
    std::vector<size_t> next(4, 0);
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < 4; ++i) consumers.emplace_back([&] { scheduler.serviceQueue(); });
    std::vector<std::thread> producers;
    for (size_t producer = 0; producer < 4; ++producer) {
        producers.emplace_back([&, producer] {
            for (size_t step = 0; step < 5000; ++step) {
                client.AddToProcessQueue([&, producer, step] {
                    if (active.fetch_add(1) != 0) ++errors;
                    if (next[producer]++ != step) ++errors;
                    --active;
                    ++completed;
                });
            }
        });
    }
    for (auto& thread : producers) thread.join();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (completed != 20000 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    scheduler.stop(true);
    for (auto& thread : consumers) thread.join();
    BOOST_CHECK_EQUAL(completed.load(), 20000U);
    BOOST_CHECK_EQUAL(errors.load(), 0U);
    BOOST_CHECK_EQUAL(Scheduled(scheduler), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
