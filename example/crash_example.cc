#include "logger.h"

#include "test_common.h"

#include <vector>

void thread_sample(SimpleLogger* ll, size_t id) {
    size_t sec = 5;
    _log_info(ll, "new thread, wait %zu seconds", sec);
    switch (id) {
    case 0:
        TestSuite::sleep_sec(sec);
        break;
    case 1:
        TestSuite::sleep_sec(sec);
        break;
    case 2:
        TestSuite::sleep_sec(sec);
        break;
    case 3:
        TestSuite::sleep_sec(sec);
        break;
    default:
        TestSuite::sleep_sec(sec);
        break;
    }
}

volatile int* ccc(int depth) {
    if (depth) return ccc(depth  - 1);

    volatile int* a = NULL;
    volatile int b = *a;
    (void)b;
    return a;
}

void bbb(char c) {
    volatile int* a = ccc(10);
    (void)a;
}

int aaa(int a, int b) {
    bbb('c');
    return a;
}

int main() {
    std::string filename = "./example_log.log";

    // If it is set to `true` (default == true), logger will display
    // the stack trace of the origin (being crashed) thread only.
    // It works only on Linux.
    SimpleLogger::setStackTraceOriginOnly(false);

    SimpleLogger* ll = new SimpleLogger(filename);
    ll->start();

    size_t num_threads = 4;
    std::vector<std::thread*> threads(num_threads);
    size_t count = 0;
    for (std::thread*& entry: threads) {
        entry = new std::thread(thread_sample, ll, count++);
    }

    TestSuite::sleep_ms(100);

    // Make crash
    aaa(1, 2);

    for (std::thread*& entry: threads) {
        if (entry->joinable()) {
            entry->join();
        }
        delete entry;
    }

    delete ll;
    SimpleLogger::shutdown();

    return 0;
}

