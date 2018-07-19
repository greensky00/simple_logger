/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Simple Logger
 * Version: 0.3.1
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
#ifdef __linux__
    #include <pthread.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef _CLM_DEFINED
#define _CLM_DEFINED (1)

#ifdef LOGGER_NO_COLOR
    #define _CLM_D_GRAY     ""
    #define _CLM_GREEN      ""
    #define _CLM_B_GREEN    ""
    #define _CLM_RED        ""
    #define _CLM_B_RED      ""
    #define _CLM_BROWN      ""
    #define _CLM_B_BROWN    ""
    #define _CLM_BLUE       ""
    #define _CLM_B_BLUE     ""
    #define _CLM_MAGENTA    ""
    #define _CLM_B_MAGENTA  ""
    #define _CLM_CYAN       ""
    #define _CLM_END        ""

    #define _CLM_WHITE_FG_RED_BG    ""
#else
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
#endif

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

#endif

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

SimpleLoggerMgr* SimpleLoggerMgr::getWithoutInit() {
    SimpleLoggerMgr* mgr = instance.load(MOR);
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

int SimpleLoggerMgr::getTzGap() {
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm* lt_tm = std::localtime(&raw_time);
    TimeInfo lt(lt_tm);
    std::tm* gmt_tm = std::gmtime(&raw_time);
    TimeInfo gmt(gmt_tm);

    return ( (  lt.day * 60 * 24 +  lt.hour * 60 +  lt.min ) -
             ( gmt.day * 60 * 24 + gmt.hour * 60 + gmt.min ) );
}

void SimpleLoggerMgr::flushCriticalInfo() {
    std::string msg = " === Critical info (given by user): ";
    msg += std::to_string(globalCriticalInfo.size()) + " bytes";
    msg += " ===";
    if (!globalCriticalInfo.empty()) {
        msg += "\n" + globalCriticalInfo;
    }
    flushAllLoggers(2, msg);
    if (crashDumpFile.is_open()) {
        crashDumpFile << msg << std::endl << std::endl;
    }
}

void SimpleLoggerMgr::_flushStackTraceBuffer(size_t buffer_len,
                                             uint32_t tid_hash,
                                             uint64_t kernel_tid,
                                             bool crash_origin)
{
    std::string msg;
    char temp_buf[256];
    sprintf(temp_buf, "\nThread %04x", tid_hash);
    msg += temp_buf;
    if (kernel_tid) {
        msg += " (" + std::to_string(kernel_tid) + ")";
    }
    if (crash_origin) {
        msg += " (crashed here)";
    }
    msg += "\n\n";
    msg += std::string(stackTraceBuffer, buffer_len);

    size_t msg_len = msg.size();
    size_t per_log_size = SimpleLogger::MSG_SIZE - 1024;
    for (size_t ii=0; ii<msg_len; ii+=per_log_size) {
        flushAllLoggers(2, msg.substr(ii, per_log_size));
    }

    if (crashDumpFile.is_open()) {
        crashDumpFile << msg << std::endl;
    }
}

void SimpleLoggerMgr::flushStackTraceBuffer(RawStackInfo& stack_info) {
    size_t len = _stack_interpret(&stack_info.stackPtrs[0],
                                  stack_info.stackPtrs.size(),
                                  stackTraceBuffer,
                                  stackTraceBufferSize);
    if (!len) return;

    _flushStackTraceBuffer(len,
                           stack_info.tidHash,
                           stack_info.kernelTid,
                           stack_info.crashOrigin);
}

void SimpleLoggerMgr::logStackBackTraceOtherThreads() {
#ifdef __linux__
    // Not support non-Linux platform.
    uint64_t my_tid = pthread_self();
    uint64_t exp = 0;
    if (!crashOriginThread.compare_exchange_strong(exp, my_tid)) {
        // Other thread is already working on it, stop here.
        return;
    }

    std::lock_guard<std::mutex> l(activeThreadsLock);
    std::string msg = "captured ";
    msg += std::to_string(activeThreads.size()) + " active threads";
    flushAllLoggers(2, msg);
    if (crashDumpFile.is_open()) crashDumpFile << msg << "\n\n";

    for (uint64_t _tid: activeThreads) {
        pthread_t tid = (pthread_t)_tid;
        if (_tid == crashOriginThread) continue;

        struct sigaction _action;
        sigfillset(&_action.sa_mask);
        _action.sa_flags = SA_SIGINFO;
        _action.sa_sigaction = SimpleLoggerMgr::handleStackTrace;
        sigaction(SIGUSR2, &_action, NULL);

        pthread_kill(tid, SIGUSR2);

        sigset_t _mask;
        sigfillset(&_mask);
        sigdelset(&_mask, SIGUSR2);
        sigsuspend(&_mask);
    }

    msg = "got all stack traces, now flushing them";
    flushAllLoggers(2, msg);
#endif

    // Got all stack trace pointers, get symbol and print it.
    for (RawStackInfo& entry: crashDumpThreadStacks) {
        flushStackTraceBuffer(entry);
    }
}

void SimpleLoggerMgr::addRawStackInfo(bool crash_origin) {
    void* stack_ptr[256];
    size_t len = _stack_backtrace(stack_ptr, 256);

    crashDumpThreadStacks.push_back(RawStackInfo());
    RawStackInfo& stack_info = *(crashDumpThreadStacks.rbegin());
    std::thread::id tid = std::this_thread::get_id();
    stack_info.tidHash = std::hash<std::thread::id>{}(tid) % 0x10000;
#ifdef __linux__
    stack_info.kernelTid = (uint64_t)syscall(SYS_gettid);
#endif
    stack_info.crashOrigin = crash_origin;
    for (size_t ii=0; ii<len; ++ii) {
        stack_info.stackPtrs.push_back(stack_ptr[ii]);
    }
}

void SimpleLoggerMgr::logStackBacktrace() {
    if (!crashDumpPath.empty() && !crashDumpFile.is_open()) {
        // Open crash dump file.
        TimeInfo lt( std::chrono::system_clock::now() );
        int tz_gap = getTzGap();
        int tz_gap_abs = (tz_gap < 0) ? (tz_gap * -1) : (tz_gap);

        char filename[128];
        sprintf(filename, "dump_%04d%02d%02d_%02d%02d%02d%c%02d%02d.txt",
                lt.year, lt.month, lt.day,
                lt.hour, lt.min, lt.sec,
                (tz_gap >= 0) ? '+' : '-',
                (int)(tz_gap_abs / 60), tz_gap_abs % 60);
        std::string path = crashDumpPath + "/" + filename;
        crashDumpFile.open(path);

        char time_fmt[64];
        sprintf(time_fmt, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%03d%c%02d:%02d",
                lt.year, lt.month, lt.day,
                lt.hour, lt.min, lt.sec, lt.msec, lt.usec,
                (tz_gap >= 0) ? '+' : '-',
                (int)(tz_gap_abs / 60), tz_gap_abs % 60);
        crashDumpFile << "When: " << time_fmt << std::endl << std::endl;
    }

    flushCriticalInfo();
    addRawStackInfo(true);
    logStackBackTraceOtherThreads();
}

void SimpleLoggerMgr::handleSegFault(int sig) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    signal(SIGSEGV, mgr->oldSigSegvHandler);
    mgr->enableOnlyOneDisplayer();
    mgr->flushAllLoggers(1, "Segmentation fault");
    mgr->logStackBacktrace();

    printf("[SEG FAULT] Flushed all logs safely.\n");
    fflush(stdout);
    if (mgr->oldSigSegvHandler) {
        mgr->oldSigSegvHandler(sig);
    }
}

void SimpleLoggerMgr::handleSegAbort(int sig) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    signal(SIGABRT, mgr->oldSigAbortHandler);
    mgr->enableOnlyOneDisplayer();
    mgr->flushAllLoggers(1, "Abort");
    mgr->logStackBacktrace();

    printf("[ABORT] Flushed all logs safely.\n");
    fflush(stdout);
    abort();
}

void SimpleLoggerMgr::handleStackTrace(int sig, siginfo_t* info, void* secret) {
#ifndef __linux__
    // Not support non-Linux platform.
    return;
#else
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (!mgr->crashOriginThread) return;

    pthread_t myself = pthread_self();
    if (mgr->crashOriginThread == myself) return;

    // NOTE:
    //   As getting exact line number is too expensive,
    //   keep stack pointers first and then interpret it.
    mgr->addRawStackInfo();

    // Go back to origin thread.
    pthread_kill(mgr->crashOriginThread, SIGUSR2);
#endif
}

void SimpleLoggerMgr::flushWorker() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    while (!mgr->chkTermination()) {
        // Every 500ms.
        mgr->sleep(500);
        mgr->flushAllLoggers();
    }
}

void SimpleLoggerMgr::setCrashDumpPath(const std::string& path) {
    crashDumpPath = path;
}


SimpleLoggerMgr::SimpleLoggerMgr()
    : termination(false)
    , oldSigSegvHandler(nullptr)
    , oldSigAbortHandler(nullptr)
    , stackTraceBuffer(nullptr)
    , crashOriginThread(0)
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

void SimpleLoggerMgr::enableOnlyOneDisplayer() {
    bool marked = false;
    std::unique_lock<std::mutex> l(loggersLock);
    for (auto& entry: loggers) {
        SimpleLogger* logger = entry;
        if (!logger) continue;
        if (!marked) {
            // The first logger: enable display
            if (logger->getLogLevel() < 4) {
                logger->setLogLevel(4);
            }
            logger->setDispLevel(4);
            marked = true;
        } else {
            // The others: disable display
            logger->setDispLevel(-1);
        }
    }
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

void SimpleLoggerMgr::addThread(uint64_t tid) {
    std::unique_lock<std::mutex> l(activeThreadsLock);
    activeThreads.insert(tid);
}

void SimpleLoggerMgr::removeThread(uint64_t tid) {
    std::unique_lock<std::mutex> l(activeThreadsLock);
    activeThreads.erase(tid);
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

const std::string& SimpleLoggerMgr::getCriticalInfo() const {
    return globalCriticalInfo;
}


// ==========================================

struct ThreadWrapper {
#ifdef __linux__
    ThreadWrapper() {
        myTid = (uint64_t)pthread_self();
        SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
        mgr->addThread(myTid);
    }
    ~ThreadWrapper() {
        SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
        mgr->removeThread(myTid);
    }
#else
    ThreadWrapper() : myTid(0) {}
    ~ThreadWrapper() {}
#endif
    uint64_t myTid;
};



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
                           uint64_t log_file_size_limit,
                           uint32_t max_log_files)
    : filePath(replaceString(file_path, "//", "/"))
    , maxLogFiles(max_log_files)
    , maxLogFileSize(log_file_size_limit)
    , numCompThreads(0)
    , curLogLevel(4)
    , curDispLevel(4)
    , tzGap( SimpleLoggerMgr::getTzGap() )
    , cursor(0)
    , logs(max_log_elems)
    , flushingLogs(false)
{
    findMinMaxRevNum(minRevnum, curRevnum);
}

SimpleLogger::~SimpleLogger() {
    stop();
    while (numCompThreads) std::this_thread::yield();
}

void SimpleLogger::setCriticalInfo(const std::string& info_str) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (mgr) {
        mgr->setCriticalInfo(info_str);
    }
}

void SimpleLogger::setCrashDumpPath(const std::string& path) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (mgr) {
        mgr->setCrashDumpPath(path);
    }
}

void SimpleLogger::shutdown() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::getWithoutInit();
    if (mgr) {
        mgr->destroy();
    }
}

std::string SimpleLogger::replaceString( const std::string& src_str,
                                         const std::string& before,
                                         const std::string& after )
{
    size_t last = 0;
    size_t pos = src_str.find(before, last);
    std::string ret;
    while (pos != std::string::npos) {
        ret += src_str.substr(last, pos - last);
        ret += after;
        last = pos + before.size();
        pos = src_str.find(before, last);
    }
    if (last < src_str.size()) {
        ret += src_str.substr(last);
    }
    return ret;
}

void SimpleLogger::findMinMaxRevNum( size_t& min_revnum_out,
                                     size_t& max_revnum_out )
{
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

    bool min_revnum_initialized = false;
    size_t min_revnum = 0;
    size_t max_revnum = 0;
    dir_info = opendir(dir_path.c_str());
    while ( dir_info && (dir_entry = readdir(dir_info)) ) {
        std::string f_name(dir_entry->d_name);
        size_t f_name_pos = f_name.rfind(file_name_only);
        // Irrelavent file: skip.
        if (f_name_pos == std::string::npos) continue;

        size_t last_dot = f_name.rfind(".");
        if (last_dot == std::string::npos) continue;

        bool comp_file = false;
        std::string ext = f_name.substr(last_dot + 1, f_name.size() - last_dot - 1);
        if (ext == "gz" && f_name.size() > 7) {
            // Compressed file: asdf.log.123.tar.gz => need to get 123.
            f_name = f_name.substr(0, f_name.size() - 7);
            last_dot = f_name.rfind(".");
            if (last_dot == std::string::npos) continue;
            ext = f_name.substr(last_dot + 1, f_name.size() - last_dot - 1);
            comp_file = true;
        }

        size_t revnum = atoi(ext.c_str());
        max_revnum = std::max( max_revnum,
                               ( (comp_file) ? (revnum+1) : (revnum) ) );
        if (!min_revnum_initialized) {
            min_revnum = revnum;
            min_revnum_initialized = true;
        }
        min_revnum = std::min(min_revnum, revnum);
    }
    closedir(dir_info);

    min_revnum_out = min_revnum;
    max_revnum_out = max_revnum;
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

    _log_sys(ll, "Start logger: %s (%zu MB per file, up to %zu files)",
             filePath.c_str(),
             maxLogFileSize / 1024 / 1024,
             maxLogFiles);

    const std::string& critical_info = mgr->getCriticalInfo();
    if (!critical_info.empty()) {
        _log_info(ll, "%s", critical_info.c_str());
    }

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

#define _snprintf(msg, avail_len, cur_len, msg_len, args...)        \
    avail_len = (avail_len > cur_len) ? (avail_len - cur_len) : 0;  \
    msg_len = snprintf( msg + cur_len, avail_len, args );           \
    cur_len += (avail_len > msg_len) ? msg_len : avail_len

#define _vsnprintf(msg, avail_len, cur_len, msg_len, args...)       \
    avail_len = (avail_len > cur_len) ? (avail_len - cur_len) : 0;  \
    msg_len = vsnprintf( msg + cur_len, avail_len, args );          \
    cur_len += (avail_len > msg_len) ? msg_len : avail_len

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
    thread_local ThreadWrapper thread_wrapper;

    // Print filename part only (excluding directory path).
    size_t last_slash = 0;
    for (size_t ii=0; source_file && source_file[ii] != 0; ++ii) {
        if (source_file[ii] == '/' || source_file[ii] == '\\') last_slash = ii;
    }

    SimpleLoggerMgr::TimeInfo lt( std::chrono::system_clock::now() );
    int tz_gap_abs = (tzGap < 0) ? (tzGap * -1) : (tzGap);

    // [time] [tid] [log type] [user msg] [stack info]
    // Timestamp: ISO 8601 format.
    size_t cur_len = 0;
    size_t avail_len = MSG_SIZE;
    size_t msg_len = 0;

    _snprintf( msg, avail_len, cur_len, msg_len,
               "%04d-%02d-%02dT%02d:%02d:%02d.%03d_%03d%c%02d:%02d "
               "[%04x] "
               "[%s] ",
               lt.year, lt.month, lt.day,
               lt.hour, lt.min, lt.sec, lt.msec, lt.usec,
               (tzGap >= 0)?'+':'-', tz_gap_abs / 60, tz_gap_abs % 60,
               tid_hash,
               lv_names[level] );

    va_list args;
    va_start(args, format);
    _vsnprintf(msg, avail_len, cur_len, msg_len, format, args);
    va_end(args);

    if (source_file && func_name) {
        _snprintf( msg, avail_len, cur_len, msg_len,
                   "\t[%s:%zu, %s()]\n",
                   source_file + ((last_slash)?(last_slash+1):0),
                   line_number, func_name );
    } else {
        _snprintf(msg, avail_len, cur_len, msg_len, "\n");
    }

    size_t num = logs.size();
    uint64_t cursor_exp = 0, cursor_val = 0;
    LogElem* ll = nullptr;
    do {
        cursor_exp = cursor.load(MOR);
        cursor_val = (cursor_exp + 1) % num;
        ll = &logs[cursor_exp];
    } while ( !cursor.compare_exchange_strong(cursor_exp, cursor_val, MOR) );
    while ( !ll->available() ) std::this_thread::yield();

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
                       { _CL_B_BROWN("===="),
                         _CL_WHITE_FG_RED_BG("FATL"),
                         _CL_B_RED("ERRO"),
                         _CL_B_MAGENTA("WARN"),
                         "INFO",
                         _CL_D_GRAY("DEBG"),
                         _CL_D_GRAY("TRAC") };

    cur_len = 0;
    avail_len = MSG_SIZE;
    _snprintf( msg, avail_len, cur_len, msg_len,
               " [" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") "."
               _CL_BROWN("%03d") " " _CL_BROWN("%03d")
               "] [tid " _CL_B_BLUE("%04x") "] "
               "[%s] ",
               lt.hour, lt.min, lt.sec, lt.msec, lt.usec,
               tid_hash,
               colored_lv_names[level] );

    if (source_file && func_name) {
        _snprintf( msg, avail_len, cur_len, msg_len,
                   "[" _CL_GREEN("%s") ":" _CL_B_RED("%zu")
                   ", " _CL_CYAN("%s()") "]\n",
                   source_file + ((last_slash)?(last_slash+1):0),
                   line_number, func_name );
    } else {
        _snprintf(msg, avail_len, cur_len, msg_len, "\n");
    }

    va_start(args, format);

#ifndef LOGGER_NO_COLOR
    if (level == 0) {
        _snprintf(msg, avail_len, cur_len, msg_len, _CLM_B_BROWN);
    } else if (level == 1) {
        _snprintf(msg, avail_len, cur_len, msg_len, _CLM_B_RED);
    }
#endif

    _vsnprintf(msg, avail_len, cur_len, msg_len, format, args);

#ifndef LOGGER_NO_COLOR
    _snprintf(msg, avail_len, cur_len, msg_len, _CLM_END);
#endif

    va_end(args);
    (void)cur_len;

    std::unique_lock<std::mutex> l(SimpleLoggerMgr::displayLock);
    std::cout << msg << std::endl;
    l.unlock();
}

void SimpleLogger::compressThread(size_t file_num) {
    int r = 0;
    std::string filename = getLogFilePath(file_num);
    std::string cmd;
    cmd = "tar zcvf " + filename + ".tar.gz " + filename +
          " > /dev/null";
    r = system(cmd.c_str());
    (void)r;

    cmd = "rm -f " + filename + " > /dev/null";
    r = system(cmd.c_str());
    (void)r;

    // Remove previous log files.
    if (maxLogFiles && file_num >= maxLogFiles) {
        for (size_t ii=minRevnum; ii<=file_num-maxLogFiles; ++ii) {
            filename = getLogFilePath(ii);
            std::string filename_tar = getLogFilePath(ii) + ".tar.gz";
            cmd = "rm -f " + filename + " " + filename_tar + " > /dev/null";
            r = system(cmd.c_str());
            (void)r;
            minRevnum = ii+1;
        }
    }

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

    if ( maxLogFileSize &&
         fs.tellp() > (int64_t)maxLogFileSize ) {
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

