#include "logger.h"

int main() {
    std::string filename = "./example_log.log";
    SimpleLogger* ll = new SimpleLogger(filename);
    ll->start();

    // 0: System  [====]
    // 1: Fatal   [FATL]
    // 2: Error   [ERRO]
    // 3: Warning [WARN]
    // 4: Info    [INFO]
    // 5: Debug   [DEBG]
    // 6: Trace   [TRAC]

    // << set log level >>
    // -1: disable
    //  0: upto System
    //  1: upto Fatal
    //  2: upto Error
    //  3: upto Warning
    //  4: upto Info
    //  5: upto Debug
    //  6: upto Trace (all)
    ll->setLogLevel(6);
    ll->setDispLevel(6);

    // printf() style.
    _log_fatal(ll, "fatal error");
    _log_err(ll, "error");
    _log_warn(ll, "warning");
    _log_info(ll, "info");
    _log_debug(ll, "debug");
    _log_trace(ll, "trace");
    _log_trace(ll, "parameters %d %d %s", 1, 2, "3");

    ll->stop();
    ll->start();

    // stream style.
    _s_fatal(ll) << "fatal error";
    _s_err(ll) << "error";
    _s_warn(ll) << "warning";
    _s_info(ll) << "info";
    _s_debug(ll) << "debug";
    _s_trace(ll) << "trace";
    _s_trace(ll) << "multi" << std::endl << "lines " << 123;

    delete ll;
    SimpleLogger::shutdown();

    return 0;
}
