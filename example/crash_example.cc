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

int main() {
    std::string filename = "./example_log.log";
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
    int* a = NULL;
    int b = *a;
    (void)b;

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
