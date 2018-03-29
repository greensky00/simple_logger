/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Simple Logger
 * Version: 0.1.15
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
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <stdarg.h>
#include <sys/time.h>

// 0: System  [====]
// 1: Fatal   [FATL]
// 2: Error   [ERRO]
// 3: Warning [WARN]
// 4: Info    [INFO]
// 5: Debug   [DEBG]
// 6: Trace   [TRAC]

#define _log_sys(l, args...) \
    if (l) (l)->put(0, __FILE__, __func__, __LINE__, args)

#define _log_fatal(l, args...) \
    if (l) (l)->put(1, __FILE__, __func__, __LINE__, args)

#define _log_err(l, args...) \
    if (l) (l)->put(2, __FILE__, __func__, __LINE__, args)

#define _log_warn(l, args...) \
    if (l) (l)->put(3, __FILE__, __func__, __LINE__, args)

#define _log_info(l, args...) \
    if (l) (l)->put(4, __FILE__, __func__, __LINE__, args)

#define _log_debug(l, args...) \
    if (l) (l)->put(5, __FILE__, __func__, __LINE__, args)

#define _log_trace(l, args...) \
    if (l) (l)->put(6, __FILE__, __func__, __LINE__, args)

class SimpleLogger {
public:
    static const int MSG_SIZE = 4096;

private:
    struct LogElem {
        enum Status {
            CLEAN = 0,
            WRITING = 1,
            DIRTY = 2,
            FLUSHING = 3,
        };

        LogElem();

        // True if dirty.
        bool needToFlush();

        // True if no other thread is working on it.
        bool available();

        int write(size_t _len, char* msg);
        int flush(std::ofstream& fs);

        size_t len;
        char ctx[MSG_SIZE];
        std::atomic<Status> status;
    };

    struct TimeInfo {
        TimeInfo(std::tm* src)
            : year(src->tm_year + 1900)
            , month(src->tm_mon + 1)
            , day(src->tm_mday)
            , hour(src->tm_hour)
            , min(src->tm_min)
            , sec(src->tm_sec) {}
        int year;
        int month;
        int day;
        int hour;
        int min;
        int sec;
    };

public:
    SimpleLogger(const std::string& file_path,
                 size_t max_log_elems = 1024,
                 uint32_t log_file_size_limit = 0);
    ~SimpleLogger();

    static void shutdown();

    static std::string replaceString
        ( const std::string& src_str,
          const std::string& before,
          const std::string& after ) {
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

    int start();
    int stop();
    inline int getLogLevel() const { return curLogLevel; }
    inline bool traceAllowed() const { return (curLogLevel >= 6); }
    inline bool debugAllowed() const { return (curLogLevel >= 5); }
    void setLogLevel(int level);
    int getDispLevel() const { return curDispLevel; }
    void setDispLevel(int level);
    void put(int level,
             const char* source_file,
             const char* func_name,
             size_t line_number,
             const char* format,
             ...);
    void flushAll();

private:
    void calcTzGap();
    size_t findLastRevNum();
    std::string getLogFilePath(size_t file_num) const;
    void compressThread(size_t file_num);
    bool flush(size_t start_pos);

    std::string filePath;
    size_t curRevnum;
    std::ofstream fs;

    uint32_t maxLogFileSize;
    std::atomic<uint32_t> numCompThreads;

    // Log up to `curLogLevel`, default: 6.
    // Disable: -1.
    int curLogLevel;

    // Display (print out on terminal) up to `curDispLevel`,
    // default: 4 (do not print debug and trace).
    // Disable: -1.
    int curDispLevel;
    std::mutex displayLock;

    int tzGap;
    std::atomic<uint64_t> cursor;
    std::vector<LogElem> logs;
    std::atomic<bool> flushingLogs;
};

// Singleton class
class SimpleLoggerMgr {
public:
    static SimpleLoggerMgr* init();
    static SimpleLoggerMgr* get();
    static void destroy();
    static void handleSegFault(int sig);
    static void handleSegAbort(int sig);
    static void flushWorker();

    void logStackBacktrace();
    void flushAllLoggers() { flushAllLoggers(0, std::string()); }
    void flushAllLoggers(int level, const std::string& msg);
    void addLogger(SimpleLogger* logger);
    void removeLogger(SimpleLogger* logger);
    void sleep(size_t ms);
    bool chkTermination() const;
    void setCriticalInfo(const std::string& info_str);
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

    std::thread tFlush;
    std::condition_variable cvSleep;
    std::mutex cvSleepLock;
    bool termination;
    void (*oldSigSegvHandler)(int);
    void (*oldSigAbortHandler)(int);

    // Critical info that will be displayed on crash.
    std::string globalCriticalInfo;

    // Reserve some buffer for stack trace.
    char* stackTraceBuffer;
};

