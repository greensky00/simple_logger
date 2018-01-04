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
    //TestSuite::clearTestFile(prefix);
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
    //TestSuite::clearTestFile(prefix);
    return 0;
}

int logger_wo_stack_info_test() {
    const std::string prefix = TEST_SUITE_AUTO_PREFIX;
    TestSuite::clearTestFile(prefix);
    std::string filename = TestSuite::getTestFileName(prefix) + ".log";

    SimpleLogger* ll = new SimpleLogger(filename, 128);
    ll->start();

    TestSuite::Timer tt(1000);

    do {
        int level = get_random_level();
        ll->put(level, nullptr, nullptr, 0, "%ld", tt.getTimeUs());
    } while(!tt.timeover());

    delete ll;

    SimpleLogger::shutdown();
    //TestSuite::clearTestFile(prefix);
    return 0;
}

int main(int argc, char** argv) {
    TestSuite ts(argc, argv);

    ts.options.printTestMessage = true;
    ts.doTest("single thread test", logger_st_test);
    ts.doTest("multi thread test", logger_mt_test);
    ts.doTest("without stack info test", logger_wo_stack_info_test);

    return 0;
}
