/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Simple Logger
 * Version: 0.1.1
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "logger.h"

#include <iostream>
#include <stdlib.h>
#include <string.h>

#define _CLM_D_GRAY     "\033[1;30m"
#define _CLM_GREEN      "\033[32m"
#define _CLM_B_GREEN    "\033[1;32m"
#define _CLM_RED        "\033[31m"
#define _CLM_B_RED      "\033[1;31m"
#define _CLM_BROWN      "\033[33m"
#define _CLM_B_BROWN    "\033[1;33m"
#define _CLM_BLUE       "\033[34m"
#define _CLM_B_BLUE     "\033[1;34m"
#define _CLM_MAGENTA    "\033[35m"
#define _CLM_B_MAGENTA  "\033[1;35m"
#define _CLM_CYAN       "\033[36m"
#define _CLM_B_GREY     "\033[1;37m"
#define _CLM_END        "\033[0m"

#define _CLM_WHITE_FG_RED_BG    "\033[37;41m"

#define _CL_D_GRAY(str)     _CLM_D_GRAY     str _CLM_END
#define _CL_GREEN(str)      _CLM_GREEN      str _CLM_END
#define _CL_RED(str)        _CLM_RED        str _CLM_END
#define _CL_B_RED(str)      _CLM_B_RED      str _CLM_END
#define _CL_MAGENTA(str)    _CLM_MAGENTA    str _CLM_END
#define _CL_BROWN(str)      _CLM_BROWN      str _CLM_END
#define _CL_B_BROWN(str)    _CLM_B_BROWN    str _CLM_END
#define _CL_B_BLUE(str)     _CLM_B_BLUE     str _CLM_END
#define _CL_B_MAGENTA(str)  _CLM_B_MAGENTA  str _CLM_END
#define _CL_CYAN(str)       _CLM_CYAN       str _CLM_END
#define _CL_B_GRAY(str)     _CLM_B_GREY     str _CLM_END

#define _CL_WHITE_FG_RED_BG(str)    _CLM_WHITE_FG_RED_BG    str _CLM_END

std::atomic<SimpleLoggerMgr*> SimpleLoggerMgr::instance(nullptr);
std::mutex SimpleLoggerMgr::instanceLock;

static const std::memory_order MOR = std::memory_order_relaxed;

SimpleLoggerMgr* SimpleLoggerMgr::init() {
    SimpleLoggerMgr* mgr = instance.load(MOR);
    if (!mgr) {
        std::lock_guard<std::mutex> l(instanceLock);
        mgr = instance.load(MOR);
        if (!mgr) {
            mgr = new SimpleLoggerMgr();
            instance.store(mgr, MOR);
        }
    }
    return mgr;
}

SimpleLoggerMgr* SimpleLoggerMgr::get() {
    SimpleLoggerMgr* mgr = instance.load(MOR);
    if (!mgr) return init();
    return mgr;
}

void SimpleLoggerMgr::destroy() {
    std::lock_guard<std::mutex> l(instanceLock);
    SimpleLoggerMgr* mgr = instance.load(MOR);
    if (mgr) {
        mgr->flushAllLoggers("Shutting down the process");
        delete mgr;
        instance.store(nullptr, MOR);
    }
}

void SimpleLoggerMgr::handleSegFault(int sig) {
    printf("SEG FAULT!!\n");
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    mgr->flushAllLoggers("Segmentation fault");
    printf("Flushed all logs safely.\n");
    fflush(stdout);
    exit(-1);
}

void SimpleLoggerMgr::flushWorker() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    while (!mgr->chkTermination()) {
        mgr->sleep(100);
        mgr->flushAllLoggers();
    }
}


SimpleLoggerMgr::SimpleLoggerMgr()
    : termination(false)
{
    oldHandler = signal(SIGSEGV, SimpleLoggerMgr::handleSegFault);
    tFlush = std::thread(SimpleLoggerMgr::flushWorker);
}

SimpleLoggerMgr::~SimpleLoggerMgr() {
    termination = true;
    std::unique_lock<std::mutex> l(cvSleepLock);
    cvSleep.notify_all();
    l.unlock();
    if (tFlush.joinable()) tFlush.join();
}

void SimpleLoggerMgr::flushAllLoggers(const std::string& msg) {
    for (auto& entry: loggers) {
        SimpleLogger* logger = entry;
        if (!msg.empty()) {
            _log_sys(logger, "%s", msg.c_str());
        }
        logger->flushAll();
    }
}

void SimpleLoggerMgr::addLogger(SimpleLogger* logger) {
    loggers.insert(logger);
}

void SimpleLoggerMgr::removeLogger(SimpleLogger* logger) {
    loggers.erase(logger);
}

void SimpleLoggerMgr::sleep(size_t ms) {
    std::unique_lock<std::mutex> l(cvSleepLock);
    cvSleep.wait_for(l, std::chrono::milliseconds(ms));
}

bool SimpleLoggerMgr::chkTermination() {
    return termination;
}


// ==========================================

SimpleLogger::LogElem::LogElem() : len(0), status(CLEAN) {}

// True if dirty.
bool SimpleLogger::LogElem::needToFlush() {
    return status.load(MOR) == DIRTY;
}

// True if no other thread is working on it.
bool SimpleLogger::LogElem::available() {
    Status s = status.load(MOR);
    return s == CLEAN || s == DIRTY;
}

int SimpleLogger::LogElem::write(size_t _len, char* msg) {
    Status exp = CLEAN;
    Status val = WRITING;
    if (!status.compare_exchange_strong(exp, val, MOR)) return -1;

    memcpy(ctx, msg, _len);
    len = _len;
    status.store(LogElem::DIRTY, MOR);
    return 0;
}

int SimpleLogger::LogElem::flush(std::ofstream& fs) {
    Status exp = DIRTY;
    Status val = FLUSHING;
    if (!status.compare_exchange_strong(exp, val, MOR)) return -1;

    fs.write(ctx, len);
    status.store(LogElem::CLEAN, MOR);
    return 0;
}


// ==========================================


SimpleLogger::SimpleLogger(const std::string file_path,
                           size_t max_log_elems)
    : filePath(file_path)
    , curLogLevel(6)
    , curDispLevel(4)
    , tzGap(0)
    , cursor(0)
    , logs(max_log_elems)
    , flushingLogs(false)
{
    calcTzGap();
}

SimpleLogger::~SimpleLogger() {
    stop();
}

void SimpleLogger::shutdown() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    mgr->destroy();
}

void SimpleLogger::calcTzGap() {
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm* lt_tm = std::localtime(&raw_time);
    TimeInfo lt(lt_tm);
    std::tm* gmt_tm = std::gmtime(&raw_time);
    TimeInfo gmt(gmt_tm);

    tzGap = (  lt.day * 60 * 24 +  lt.hour * 60 +  lt.min ) -
            ( gmt.day * 60 * 24 + gmt.hour * 60 + gmt.min );
}

int SimpleLogger::start() {
    if (filePath.empty()) return 0;

    // Append at the end.
    fs.open(filePath.c_str(), std::ofstream::out | std::ofstream::app);
    if (!fs) return -1;

    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    mgr->addLogger(this);

    _log_sys(this, "Start logger: %s", filePath.c_str());

    return 0;
}

int SimpleLogger::stop() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    mgr->removeLogger(this);

    _log_sys(this, "Stop logger: %s", filePath.c_str());
    flushAll();
    if (fs) {
        fs.flush();
        fs.close();
    }

    return 0;
}

void SimpleLogger::setLogLevel(int level) {
    if (level > 6) return;
    if (!fs) return;
    curLogLevel = level;
}

void SimpleLogger::setDispLevel(int level) {
    if (level > 6) return;
    curDispLevel = level;
}

void SimpleLogger::put(int level,
                       const char* source_file,
                       const char* func_name,
                       size_t line_number,
                       const char* format,
                       ...)
{
    if (level > curLogLevel) return;
    if (!fs) return;

    static const char* lv_names[7] = {"====",
                                      "FATL", "ERRO", "WARN",
                                      "INFO", "DEBG", "TRAC"};
    thread_local char msg[MSG_SIZE];
    thread_local std::thread::id tid = std::this_thread::get_id();
    thread_local uint32_t tid_hash = std::hash<std::thread::id>{}(tid) % 0x10000;

    // Print filename part only (excluding directory path).
    size_t last_slash = 0;
    for (size_t ii=0; source_file && source_file[ii] != 0; ++ii) {
        if (source_file[ii] == '/' || source_file[ii] == '\\') last_slash = ii;
    }

    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm* lt_tm = std::localtime(&raw_time);
    TimeInfo lt(lt_tm);

    int tz_gap_abs = (tzGap < 0) ? (tzGap * -1) : (tzGap);
    int ms = (int)( std::chrono::duration_cast
                        <std::chrono::milliseconds>
                            ( now.time_since_epoch() ).count() % 1000 );

    // [time] [tid] [log type] [user msg] [stack info]
    // Timestamp: ISO 8601 format.
    size_t cur_len = 0;
    cur_len += snprintf( msg, MSG_SIZE - cur_len,
                         "%04d-%02d-%02dT%02d:%02d:%02d.%03d%c%02d:%02d "
                         "[%04x] "
                         "[%s] ",
                         lt.year, lt.month, lt.day,
                         lt.hour, lt.min, lt.sec, ms,
                         (tzGap >= 0)?'+':'-', tz_gap_abs / 60, tz_gap_abs % 60,
                         tid_hash,
                         lv_names[level] );
    va_list args;
    va_start(args, format);
    cur_len += vsnprintf(msg + cur_len, MSG_SIZE - cur_len, format, args);
    va_end(args);

    if (source_file && func_name) {
        cur_len += snprintf( msg + cur_len, MSG_SIZE - cur_len, "\t[%s:%zu, %s()]\n",
                             source_file + ((last_slash)?(last_slash+1):0),
                             line_number, func_name );
    } else {
        cur_len += snprintf(msg + cur_len, MSG_SIZE - cur_len, "\n");
    }

    size_t num = logs.size();
    uint64_t cursor_exp, cursor_val;
    LogElem* ll = nullptr;
    do {
        cursor_exp = cursor.load(MOR);
        cursor_val = (cursor_exp + 1) % num;
        ll = &logs[cursor_exp];
    } while ( !cursor.compare_exchange_strong(cursor_exp, cursor_val, MOR) ||
              !ll->available() );

    if (ll->needToFlush()) {
        // Allow only one thread to flush.
        if (!flush(cursor_exp)) {
            // Other threads: wait.
            while (ll->needToFlush()) std::this_thread::yield();
        }
    }
    ll->write(cur_len, msg);

    if (level > curDispLevel) return;

    // Console part.
    static const char* colored_lv_names[7] =
        {_CL_B_BROWN("===="), _CL_WHITE_FG_RED_BG("FATL"), _CL_B_RED("ERRO"),
         _CL_B_MAGENTA("WARN"), "INFO",
         _CL_D_GRAY("DEBG"), _CL_D_GRAY("TRAC")};

    int us = (int)( std::chrono::duration_cast
                        <std::chrono::microseconds>
                            ( now.time_since_epoch() ).count() % 1000 );

    cur_len = 0;
    cur_len +=
        snprintf( msg, MSG_SIZE - cur_len,
                  "[" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") "."
                  _CL_BROWN("%03d") " " _CL_BROWN("%03d")
                  "] [tid " _CL_B_BLUE("%04x") "] "
                  "[%s] ",
                  lt.hour, lt.min, lt.sec, ms, us,
                  tid_hash,
                  colored_lv_names[level] );

    if (source_file && func_name) {
        cur_len +=
            snprintf( msg + cur_len, MSG_SIZE - cur_len,
                      "[" _CL_GREEN("%s") ":" _CL_B_RED("%zu")
                      ",\t" _CL_CYAN("%s()") "]\n",
                      source_file + ((last_slash)?(last_slash+1):0),
                      line_number, func_name );
    } else {
        cur_len += snprintf(msg + cur_len, MSG_SIZE - cur_len, "\n");
    }

    va_start(args, format);
    if (level == 0) {
        cur_len += snprintf(msg + cur_len, MSG_SIZE - cur_len, _CLM_B_BROWN);
    } else if (level == 1) {
        cur_len += snprintf(msg + cur_len, MSG_SIZE - cur_len, _CLM_B_RED);
    }
    cur_len += vsnprintf(msg + cur_len, MSG_SIZE - cur_len, format, args);
    cur_len += snprintf(msg + cur_len, MSG_SIZE - cur_len, _CLM_END);
    va_end(args);

    std::unique_lock<std::mutex> l(displayLock);
    std::cout << msg << std::endl;
    l.unlock();
}

bool SimpleLogger::flush(size_t start_pos) {
    bool exp = false;
    bool val = true;
    if (!flushingLogs.compare_exchange_strong(exp, val, MOR)) return false;

    size_t num = logs.size();
    // Circular flush into file.
    for (size_t ii=start_pos; ii<num; ++ii) {
        LogElem& ll = logs[ii];
        ll.flush(fs);
    }
    for (size_t ii=0; ii<start_pos; ++ii) {
        LogElem& ll = logs[ii];
        ll.flush(fs);
    }
    fs.flush();
    flushingLogs.store(false, MOR);
    return true;
}

void SimpleLogger::flushAll() {
    uint64_t start_pos = cursor.load(MOR);
    flush(start_pos);
}

