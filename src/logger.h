/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Simple Logger
 * Version: 0.3.5
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

#pragma once

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <stdarg.h>
#include <sys/time.h>

// To suppress false alarms by thread sanitizer,
// add -DSUPPRESS_TSAN_FALSE_ALARMS=1 flag to CXXFLAGS.
//#define SUPPRESS_TSAN_FALSE_ALARMS (1)

// 0: System  [====]
// 1: Fatal   [FATL]
// 2: Error   [ERRO]
// 3: Warning [WARN]
// 4: Info    [INFO]
// 5: Debug   [DEBG]
// 6: Trace   [TRAC]


// printf style log macro
#define _log_(level, l, args...)        \
    if (l && l->getLogLevel() >= level) \
        (l)->put(level, __FILE__, __func__, __LINE__, args)

#define _log_sys(l, args...)    _log_(SimpleLogger::SYS,     l, args)
#define _log_fatal(l, args...)  _log_(SimpleLogger::FATAL,   l, args)
#define _log_err(l, args...)    _log_(SimpleLogger::ERROR,   l, args)
#define _log_warn(l, args...)   _log_(SimpleLogger::WARNING, l, args)
#define _log_info(l, args...)   _log_(SimpleLogger::INFO,    l, args)
#define _log_debug(l, args...)  _log_(SimpleLogger::DEBUG,   l, args)
#define _log_trace(l, args...)  _log_(SimpleLogger::TRACE,   l, args)


// stream log macro
#define _stream_(level, l)              \
    if (l && l->getLogLevel() >= level) \
        l->eos() = l->stream(level, l, __FILE__, __func__, __LINE__)

#define _s_sys(l)   _stream_(SimpleLogger::SYS,     l)
#define _s_fatal(l) _stream_(SimpleLogger::FATAL,   l)
#define _s_err(l)   _stream_(SimpleLogger::ERROR,   l)
#define _s_warn(l)  _stream_(SimpleLogger::WARNING, l)
#define _s_info(l)  _stream_(SimpleLogger::INFO,    l)
#define _s_debug(l) _stream_(SimpleLogger::DEBUG,   l)
#define _s_trace(l) _stream_(SimpleLogger::TRACE,   l)


class SimpleLoggerMgr;
class SimpleLogger {
    friend class SimpleLoggerMgr;
public:
    static const int MSG_SIZE = 4096;

    enum Levels {
        SYS         = 0,
        FATAL       = 1,
        ERROR       = 2,
        WARNING     = 3,
        INFO        = 4,
        DEBUG       = 5,
        TRACE       = 6,
    };

    class LoggerStream : public std::ostream {
    public:
        LoggerStream() : std::ostream(&buf), level(0), logger(nullptr)
                       , file(nullptr), func(nullptr), line(0) {}

        template<typename T>
        inline LoggerStream& operator<<(const T& data) {
            sStream << data;
            return *this;
        }

        inline void put() {
            if (logger) {
                logger->put( level, file, func, line,
                             "%s", sStream.str().c_str() );
            }
        }

        inline void setLogInfo(int _level,
                               SimpleLogger* _logger,
                               const char* _file,
                               const char* _func,
                               size_t _line)
        {
            sStream.str(std::string());
            level = _level;
            logger = _logger;
            file = _file;
            func = _func;
            line = _line;
        }

    private:
        std::stringbuf buf;
        std::stringstream sStream;
        int level;
        SimpleLogger* logger;
        const char* file;
        const char* func;
        size_t line;
    };

    class EndOfStmt {
    public:
        EndOfStmt() {}
        EndOfStmt(LoggerStream& src) { src.put(); }
        EndOfStmt& operator=(LoggerStream& src) { src.put(); return *this; }
    };

    LoggerStream& stream( int level,
                          SimpleLogger* logger,
                          const char* file,
                          const char* func,
                          size_t line ) {
        thread_local LoggerStream msg;
        msg.setLogInfo(level, logger, file, func, line);
        return msg;
    }

    EndOfStmt& eos() {
        thread_local EndOfStmt _eos;
        return _eos;
    }

private:
    struct LogElem {
        enum Status {
            CLEAN       = 0,
            WRITING     = 1,
            DIRTY       = 2,
            FLUSHING    = 3,
        };

        LogElem();

        // True if dirty.
        bool needToFlush();

        // True if no other thread is working on it.
        bool available();

        int write(size_t _len, char* msg);
        int flush(std::ofstream& fs);

#ifdef SUPPRESS_TSAN_FALSE_ALARMS
        // To avoid false alarm by TSan.
        std::mutex ctxLock;
#endif
        size_t len;
        char ctx[MSG_SIZE];
        std::atomic<Status> status;
    };

public:
    SimpleLogger(const std::string& file_path,
                 size_t max_log_elems           = 4096,
                 uint64_t log_file_size_limit   = 32*1024*1024,
                 uint32_t max_log_files         = 16);
    ~SimpleLogger();

    static void setCriticalInfo(const std::string& info_str);
    static void setCrashDumpPath(const std::string& path);

    static void shutdown();
    static std::string replaceString(const std::string& src_str,
                                     const std::string& before,
                                     const std::string& after);

    int start();
    int stop();

    inline bool traceAllowed() const { return (curLogLevel >= 6); }
    inline bool debugAllowed() const { return (curLogLevel >= 5); }

    void setLogLevel(int level);
    void setDispLevel(int level);

    inline int getLogLevel()  const { return curLogLevel; }
    inline int getDispLevel() const { return curDispLevel; }

    void put(int level,
             const char* source_file,
             const char* func_name,
             size_t line_number,
             const char* format,
             ...);
    void flushAll();

private:
    void calcTzGap();
    void findMinMaxRevNum(size_t& min_revnum_out,
                          size_t& max_revnum_out);
    std::string getLogFilePath(size_t file_num) const;
    void compressThread(size_t file_num);
    bool flush(size_t start_pos);

    std::string filePath;
    size_t minRevnum;
    size_t curRevnum;
    size_t maxLogFiles;
    std::ofstream fs;

    uint64_t maxLogFileSize;
    std::atomic<uint32_t> numCompThreads;

    // Log up to `curLogLevel`, default: 6.
    // Disable: -1.
#ifdef SUPPRESS_TSAN_FALSE_ALARMS
    std::atomic<int> curLogLevel;
#else
    int curLogLevel;
#endif
    // Display (print out on terminal) up to `curDispLevel`,
    // default: 4 (do not print debug and trace).
    // Disable: -1.
#ifdef SUPPRESS_TSAN_FALSE_ALARMS
    std::atomic<int> curDispLevel;
#else
    int curDispLevel;
#endif
    std::mutex displayLock;

    int tzGap;
    std::atomic<uint64_t> cursor;
    std::vector<LogElem> logs;
    std::atomic<bool> flushingLogs;
};

// Singleton class
class SimpleLoggerMgr {
public:
    struct TimeInfo {
        TimeInfo(std::tm* src)
            : year(src->tm_year + 1900)
            , month(src->tm_mon + 1)
            , day(src->tm_mday)
            , hour(src->tm_hour)
            , min(src->tm_min)
            , sec(src->tm_sec)
            , msec(0)
            , usec(0) {}
        TimeInfo(std::chrono::system_clock::time_point now) {
            std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
            std::tm* lt_tm = std::localtime(&raw_time);
            year = lt_tm->tm_year + 1900;
            month = lt_tm->tm_mon + 1;
            day = lt_tm->tm_mday;
            hour = lt_tm->tm_hour;
            min = lt_tm->tm_min;
            sec = lt_tm->tm_sec;

            size_t us_epoch = std::chrono::duration_cast
                              < std::chrono::microseconds >
                              ( now.time_since_epoch() ).count();
            msec = (us_epoch / 1000) % 1000;
            usec = us_epoch % 1000;
        }
        int year;
        int month;
        int day;
        int hour;
        int min;
        int sec;
        int msec;
        int usec;
    };

    struct RawStackInfo {
        RawStackInfo() : tidHash(0), kernelTid(0), crashOrigin(false) {}
        uint32_t tidHash;
        uint64_t kernelTid;
        std::vector<void*> stackPtrs;
        bool crashOrigin;
    };

    static SimpleLoggerMgr* init();
    static SimpleLoggerMgr* get();
    static SimpleLoggerMgr* getWithoutInit();
    static void destroy();
    static int getTzGap();
    static void handleSegFault(int sig);
    static void handleSegAbort(int sig);
    static void handleStackTrace(int sig, siginfo_t* info, void* secret);
    static void flushWorker();

    void addRawStackInfo(bool crash_origin = false);
    void logStackBackTraceOtherThreads();
    void logStackBacktrace();
    void flushCriticalInfo();
    void _flushStackTraceBuffer(size_t buffer_len,
                                uint32_t tid_hash,
                                uint64_t kernel_tid,
                                bool crash_origin);
    void flushStackTraceBuffer(RawStackInfo& stack_info);
    void enableOnlyOneDisplayer();
    void flushAllLoggers() { flushAllLoggers(0, std::string()); }
    void flushAllLoggers(int level, const std::string& msg);
    void addLogger(SimpleLogger* logger);
    void removeLogger(SimpleLogger* logger);
    void addThread(uint64_t tid);
    void removeThread(uint64_t tid);
    void sleep(size_t ms);
    bool chkTermination() const;
    void setCriticalInfo(const std::string& info_str);
    void setCrashDumpPath(const std::string& path);
    const std::string& getCriticalInfo() const;

    static std::mutex displayLock;

private:
    // Copy is not allowed.
    SimpleLoggerMgr(const SimpleLoggerMgr&) = delete;
    SimpleLoggerMgr& operator=(const SimpleLoggerMgr&) = delete;

    static const size_t stackTraceBufferSize = 65536;

    // Singleton instance and lock.
    static std::atomic<SimpleLoggerMgr*> instance;
    static std::mutex instanceLock;

    SimpleLoggerMgr();
    ~SimpleLoggerMgr();

    std::mutex loggersLock;
    std::unordered_set<SimpleLogger*> loggers;

    std::mutex activeThreadsLock;
    std::unordered_set<uint64_t> activeThreads;

    std::thread tFlush;
    std::condition_variable cvSleep;
    std::mutex cvSleepLock;
    std::atomic<bool> termination;
    void (*oldSigSegvHandler)(int);
    void (*oldSigAbortHandler)(int);

    // Critical info that will be displayed on crash.
    std::string globalCriticalInfo;

    // Reserve some buffer for stack trace.
    char* stackTraceBuffer;

    // TID of thread where crash happens.
    std::atomic<uint64_t> crashOriginThread;

    std::string crashDumpPath;
    std::ofstream crashDumpFile;

    std::atomic<uint64_t> abortTimer;

    // Assume that only one thread is updating this.
    std::vector<RawStackInfo> crashDumpThreadStacks;
};

