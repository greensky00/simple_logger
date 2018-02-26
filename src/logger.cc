/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Simple Logger
 * Version: 0.1.11
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

#include "backtrace.h"

#include <iostream>

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
std::mutex SimpleLoggerMgr::displayLock;

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
        mgr->flushAllLoggers();
        delete mgr;
        instance.store(nullptr, MOR);
    }
}

void SimpleLoggerMgr::logStackBacktrace() {
    size_t len = stack_backtrace(stackTraceBuffer, stackTraceBufferSize);
    if (len) {
        std::string msg = "\n";
        msg += globalCriticalInfo + "\n\n";
        msg += stackTraceBuffer;
        flushAllLoggers(2, msg);
    }
}

void SimpleLoggerMgr::handleSegFault(int sig) {
    printf("SEG FAULT!!\n");
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    signal(SIGSEGV, mgr->oldSigSegvHandler);
    mgr->flushAllLoggers(1, "Segmentation fault");
    mgr->logStackBacktrace();

    printf("Flushed all logs safely.\n");
    fflush(stdout);
    mgr->oldSigSegvHandler(sig);
}

void SimpleLoggerMgr::handleSegAbort(int sig) {
    printf("ABORT!!\n");
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    signal(SIGABRT, mgr->oldSigAbortHandler);
    mgr->flushAllLoggers(1, "Abort");
    mgr->logStackBacktrace();

    printf("Flushed all logs safely.\n");
    fflush(stdout);
    abort();
}

void SimpleLoggerMgr::flushWorker() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    while (!mgr->chkTermination()) {
        // Every 500ms.
        mgr->sleep(500);
        mgr->flushAllLoggers();
    }
}


SimpleLoggerMgr::SimpleLoggerMgr()
    : termination(false)
    , oldSigSegvHandler(nullptr)
    , oldSigAbortHandler(nullptr)
    , stackTraceBuffer(nullptr)
{
    oldSigSegvHandler = signal(SIGSEGV, SimpleLoggerMgr::handleSegFault);
    oldSigAbortHandler = signal(SIGABRT, SimpleLoggerMgr::handleSegAbort);
    tFlush = std::thread(SimpleLoggerMgr::flushWorker);

    stackTraceBuffer = (char*)malloc(stackTraceBufferSize);
}

SimpleLoggerMgr::~SimpleLoggerMgr() {
    termination = true;

    signal(SIGSEGV, oldSigSegvHandler);
    signal(SIGABRT, oldSigAbortHandler);

    std::unique_lock<std::mutex> l(cvSleepLock);
    cvSleep.notify_all();
    l.unlock();
    if (tFlush.joinable()) {
        tFlush.join();
    }

    free(stackTraceBuffer);
}

void SimpleLoggerMgr::flushAllLoggers(int level, const std::string& msg) {
    std::unique_lock<std::mutex> l(loggersLock);
    for (auto& entry: loggers) {
        SimpleLogger* logger = entry;
        if (!logger) continue;
        if (!msg.empty()) {
            logger->put(level, __FILE__, __func__, __LINE__, "%s", msg.c_str());
        }
        logger->flushAll();
    }
}

void SimpleLoggerMgr::addLogger(SimpleLogger* logger) {
    std::unique_lock<std::mutex> l(loggersLock);
    loggers.insert(logger);
}

void SimpleLoggerMgr::removeLogger(SimpleLogger* logger) {
    std::unique_lock<std::mutex> l(loggersLock);
    loggers.erase(logger);
}

void SimpleLoggerMgr::sleep(size_t ms) {
    std::unique_lock<std::mutex> l(cvSleepLock);
    cvSleep.wait_for(l, std::chrono::milliseconds(ms));
}

bool SimpleLoggerMgr::chkTermination() const {
    return termination;
}

void SimpleLoggerMgr::setCriticalInfo(const std::string& info_str) {
    globalCriticalInfo = info_str;
}



// ==========================================

SimpleLogger::LogElem::LogElem() : len(0), status(CLEAN) {
    memset(ctx, 0x0, MSG_SIZE);
}

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

    len = (_len > MSG_SIZE) ? MSG_SIZE : _len;
    memcpy(ctx, msg, len);
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


SimpleLogger::SimpleLogger(const std::string& file_path,
                           size_t max_log_elems,
                           uint32_t log_file_size_limit)
    : filePath(replaceString(file_path, "//", "/"))
    , curRevnum(findLastRevNum())
    , maxLogFileSize(log_file_size_limit)
    , numCompThreads(0)
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
    while (numCompThreads) std::this_thread::yield();
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

size_t SimpleLogger::findLastRevNum() {
    DIR *dir_info;
    struct dirent *dir_entry;

    std::string dir_path = "./";
    std::string file_name_only = filePath;
    size_t last_pos = filePath.rfind("/");
    if (last_pos != std::string::npos) {
        dir_path = filePath.substr(0, last_pos);
        file_name_only = filePath.substr
                         ( last_pos + 1, filePath.size() - last_pos - 1 );
    }

    size_t max_revnum = 0;
    dir_info = opendir(dir_path.c_str());
    while ( dir_info && (dir_entry = readdir(dir_info)) ) {
        std::string f_name(dir_entry->d_name);
        size_t f_name_pos = f_name.rfind(file_name_only);
        // Irrelavent file: skip.
        if (f_name_pos == std::string::npos) continue;

        size_t last_dot = f_name.rfind(".");
        if (last_dot == std::string::npos) continue;

        std::string ext = f_name.substr(last_dot + 1, f_name.size() - last_dot - 1);
        size_t revnum = atoi(ext.c_str());
        max_revnum = std::max(max_revnum, revnum);
    }
    closedir(dir_info);
    return max_revnum;
}

std::string SimpleLogger::getLogFilePath(size_t file_num) const {
    if (file_num) {
        return filePath + "." + std::to_string(file_num);
    }
    return filePath;
}

int SimpleLogger::start() {
    if (filePath.empty()) return 0;

    // Append at the end.
    fs.open(getLogFilePath(curRevnum), std::ofstream::out | std::ofstream::app);
    if (!fs) return -1;

    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    SimpleLogger* ll = this;
    mgr->addLogger(ll);

    _log_sys(ll, "Start logger: %s", filePath.c_str());

    return 0;
}

int SimpleLogger::stop() {
    if (fs.is_open()) {
        SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
        SimpleLogger* ll = this;
        mgr->removeLogger(ll);

        _log_sys(ll, "Stop logger: %s", filePath.c_str());
        flushAll();
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
                  " [" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") "."
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
                      ", " _CL_CYAN("%s()") "]\n",
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

    std::unique_lock<std::mutex> l(SimpleLoggerMgr::displayLock);
    std::cout << msg << std::endl;
    l.unlock();
}

void SimpleLogger::compressThread(size_t file_num) {
    int r = 0;
    std::string filename = getLogFilePath(file_num);
    std::string cmd;
    cmd = "tar zcvf " + filename + ".tar.gz " + filename;
    r = system(cmd.c_str());
    (void)r;

    cmd = "rm -rf " + filename;
    r = system(cmd.c_str());
    (void)r;

    numCompThreads.fetch_sub(1);
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

    if (maxLogFileSize && fs.tellp() > maxLogFileSize) {
        // Exceeded limit, make a new file.
        curRevnum++;
        fs.close();
        fs.open(getLogFilePath(curRevnum), std::ofstream::out | std::ofstream::app);

        // Compress it (tar gz)
        numCompThreads.fetch_add(1);
        std::thread t(&SimpleLogger::compressThread, this, curRevnum-1);
        t.detach();
    }

    flushingLogs.store(false, MOR);
    return true;
}

void SimpleLogger::flushAll() {
    uint64_t start_pos = cursor.load(MOR);
    flush(start_pos);
}

