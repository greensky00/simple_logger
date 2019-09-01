#include "logger.h"

#include "test_common.h"

int get_random_level() {
    size_t n = std::rand() & 0xffffff;
    if (n < 16) return 1;
    else if (n < 32) return 2;
    else if (n < 128) return 3;
    else if (n < 512) return 4;
    return 5;
}

int logger_st_test() {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    SimpleLogger* ll = new SimpleLogger(filename, 128);
    ll->start();
    ll->setLogLevel(6);

    TestSuite::Timer tt(1000);

    do {
        int level = get_random_level();
        if (level == 1) {
            _log_fatal(ll, "%ld", tt.getTimeUs());
        } else if (level == 2) {
            _log_err(ll, "%ld", tt.getTimeUs());
        } else if (level == 3) {
            _log_warn(ll, "%ld", tt.getTimeUs());
        } else if (level == 4) {
            _log_info(ll, "%ld", tt.getTimeUs());
        } else if (level == 5) {
            _log_debug(ll, "%ld", tt.getTimeUs());
        }
    } while(!tt.timeover());

    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

struct WorkerArgs {
    SimpleLogger* ll;
    TestSuite::Timer* tt;
};

void logger_worker(WorkerArgs* args) {
    SimpleLogger* ll = args->ll;
    TestSuite::Timer& tt = *args->tt;
    do {
        int level = get_random_level();
        if (level == 1) {
            _log_fatal(ll, "%ld", tt.getTimeUs());
        } else if (level == 2) {
            _log_err(ll, "%ld", tt.getTimeUs());
        } else if (level == 3) {
            _log_warn(ll, "%ld", tt.getTimeUs());
        } else if (level == 4) {
            _log_info(ll, "%ld", tt.getTimeUs());
        } else if (level == 5) {
            _log_debug(ll, "%ld", tt.getTimeUs());
        }
    } while(!tt.timeover());
}

int logger_mt_test() {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    SimpleLogger* ll = new SimpleLogger(filename, 128);
    ll->start();
    ll->setLogLevel(6);

    TestSuite::Timer tt(1000);
    size_t num_cores = std::thread::hardware_concurrency();
    if (num_cores < 2) num_cores = 2;

    std::thread tid[num_cores];
    WorkerArgs args[num_cores];
    for (size_t ii=0; ii<num_cores; ++ii) {
        args[ii].ll = ll;
        args[ii].tt = &tt;
        tid[ii] = std::thread(logger_worker, &args[ii]);
    }
    for (size_t ii=0; ii<num_cores; ++ii) {
        if (tid[ii].joinable()) tid[ii].join();
    }

    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int logger_wo_stack_info_test() {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    SimpleLogger* ll = new SimpleLogger(filename, 128);
    ll->start();
    ll->setLogLevel(6);

    TestSuite::Timer tt(1000);

    do {
        int level = get_random_level();
        ll->put(level, nullptr, nullptr, 0, "%ld", tt.getTimeUs());
    } while(!tt.timeover());

    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int logger_split_comp_test(uint64_t num) {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    uint64_t ii=0;
    SimpleLogger* ll = nullptr;
    TestSuite::Timer tt;

    ll = new SimpleLogger(filename, 128, 32*1024*1024);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num/2; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    ll = new SimpleLogger(filename, 128, 32*1024*1024);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int logger_reopen_with_split_option_test(uint64_t num) {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    uint64_t ii=0;
    SimpleLogger* ll = nullptr;
    TestSuite::Timer tt;

    ll = new SimpleLogger(filename, 128, 256*1024*1024);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num/2; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    ll = new SimpleLogger(filename, 128, 32*1024*1024);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int logger_reclaim_test(uint64_t num) {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    uint64_t ii=0;
    SimpleLogger* ll = nullptr;
    TestSuite::Timer tt;

    ll = new SimpleLogger(filename, 128, 1024*1024, 4);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num/2; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    ll = new SimpleLogger(filename, 128, 1024*1024, 4);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int logger_reopen_with_reclaim_option_test(uint64_t num) {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    uint64_t ii=0;
    SimpleLogger* ll = nullptr;
    TestSuite::Timer tt;

    ll = new SimpleLogger(filename, 128, 1024*1024, 0);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num/2; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    ll = new SimpleLogger(filename, 128, 1024*1024, 4);
    ll->start();
    ll->setLogLevel(6);
    for (; ii<num; ++ii) {
        if (ii && ii % 100000 == 0) {
            _log_info(ll, "%ld: %ld", ii, tt.getTimeUs());
        } else {
            _log_trace(ll, "%ld: %ld", ii, tt.getTimeUs());
        }
    }
    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int function_parameter(int logger_level, int target_level) {
    // If logger's log level is smaller than target log's level,
    // this function shouldn't be called.
    if (target_level > logger_level) {
        CHK_SMEQ(target_level, logger_level);
    }
    return target_level;
}

int logger_function_as_parameter_test() {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    SimpleLogger* ll = nullptr;
    TestSuite::Timer tt;

    ll = new SimpleLogger(filename, 128, 1024*1024, 0);
    ll->start();
    ll->setLogLevel(6);

    for (int ii=1; ii<=6; ++ii) {
        ll->setLogLevel(ii);
        ll->setDispLevel(ii);
        for (int jj=1; jj<=6; ++jj) {
            _log_(jj, ll,
                  "logger level %d, target level %d",
                  ii, function_parameter(ii, jj));
            _stream_(jj, ll)
                << "logger level " << ii << ", "
                << "target level " << function_parameter(ii, jj);
        }
    }

    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int logger_timed_log_test(bool is_global) {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    SimpleLogger* ll = nullptr;
    TestSuite::Timer tt;

    ll = new SimpleLogger(filename, 128, 1024*1024, 0);
    ll->start();
    ll->setLogLevel(6);

    auto worker_func = [&ll, is_global](int tid) -> void {
        for (size_t ii=0; ii<100; ++ii) {
            SimpleLogger::Levels lv = (ii < 50)
                                      ? SimpleLogger::TRACE
                                      : SimpleLogger::UNKNOWN;
            if (is_global) {
                _timed_log_g(ll, 100, lv, SimpleLogger::INFO,
                             "thread %d, %zu", tid, ii);
            } else {
                _timed_log_t(ll, 100, lv, SimpleLogger::INFO,
                             "thread %d %zu", tid, ii);
            }
            TestSuite::sleep_ms(10);
        }
    };

    std::thread t1(worker_func, 1);
    std::thread t2(worker_func, 2);
    if (t1.joinable()) t1.join();
    if (t2.joinable()) t2.join();

    delete ll;

    SimpleLogger::shutdown();
    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int logger_init_twice_test() {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    SimpleLogger* ll = nullptr;
    TestSuite::Timer tt;

    ll = new SimpleLogger(filename, 128, 1024*1024, 0);
    ll->start();
    delete ll;
    SimpleLogger::shutdown();

    ll = new SimpleLogger(filename, 128, 1024*1024, 0);
    ll->start();
    delete ll;
    SimpleLogger::shutdown();

#if 0
    int* a = 0;
    int b = *a;
    (void)b;
#endif

    TestSuite::clearTestFile(prefix, TestSuite::END_OF_TEST);
    return 0;
}

int main(int argc, char** argv) {
    TestSuite ts(argc, argv);

    ts.options.printTestMessage = true;
    ts.doTest("single thread test", logger_st_test);

    ts.doTest("multi thread test", logger_mt_test);

    ts.doTest("without stack info test", logger_wo_stack_info_test);

    ts.doTest("split compression test",
              logger_split_comp_test,
              TestRange<uint64_t>({(uint64_t)1000000}));

    ts.doTest("reopen with split option test",
              logger_reopen_with_split_option_test,
              TestRange<uint64_t>({(uint64_t)1000000}));

    ts.doTest("reclaim basic test",
              logger_reclaim_test,
              TestRange<uint64_t>({(uint64_t)1000000}));

    ts.doTest("reopen with reclaim option test",
              logger_reopen_with_reclaim_option_test,
              TestRange<uint64_t>({(uint64_t)1000000}));

    ts.doTest("function as parameter test",
              logger_function_as_parameter_test);

    ts.doTest("timed log test",
              logger_timed_log_test,
              TestRange<bool>({true, false}));

    ts.doTest("logger init twice test",
              logger_init_twice_test);

    return 0;
}
