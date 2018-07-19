/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Stack Backtrace
 * Version: 0.2.0
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

// LCOV_EXCL_START

#define SIZE_T_UNUSED size_t __attribute__((unused))

#include <cstddef>
#include <string>

#include <cxxabi.h>
#include <execinfo.h>
#include <inttypes.h>
#include <stdio.h>
#include <signal.h>

#define _snprintf(msg, avail_len, cur_len, msg_len, args...)        \
    avail_len = (avail_len > cur_len) ? (avail_len - cur_len) : 0;  \
    msg_len = snprintf( msg + cur_len, avail_len, args );           \
    cur_len += (avail_len > msg_len) ? msg_len : avail_len

static SIZE_T_UNUSED
_stack_backtrace(void** stack_ptr, size_t stack_ptr_capacity) {
    return backtrace(stack_ptr, stack_ptr_capacity);
}

static SIZE_T_UNUSED
_stack_interpret(void** stack_ptr,
                 int stack_size,
                 char* output_buf,
                 size_t output_buflen)
{
    char** stack_msg = nullptr;
    stack_msg = backtrace_symbols(stack_ptr, stack_size);

    size_t cur_len = 0;

    size_t frame_num = 0;
    (void)frame_num;

    // NOTE: starting from 1, skipping this frame.
    for (int i=1; i<stack_size; ++i) {

#ifdef __linux__
        int fname_len = 0;
        while ( stack_msg[i][fname_len] != '(' &&
                stack_msg[i][fname_len] != ' ' &&
                stack_msg[i][fname_len] != 0 ) {
            ++fname_len;
        }

        char cmd[1024];
        snprintf(cmd, 1024, "addr2line -f -e %.*s %" PRIxPTR "",
                 fname_len, stack_msg[i], (uintptr_t)stack_ptr[i] );
        FILE* fp = popen(cmd, "r");
        if (!fp) continue;

        char mangled_name[1024];
        char file_line[1024];
        int ret = fscanf(fp, "%1023s %1023s", mangled_name, file_line);
        (void)ret;
        pclose(fp);

        size_t msg_len = 0;
        size_t avail_len = output_buflen;
        _snprintf(output_buf, avail_len, cur_len, msg_len,
                  "#%-2zu 0x%016" PRIxPTR " in ",
                  frame_num++, (uintptr_t)stack_ptr[i] );

        int status;
        char *cc = abi::__cxa_demangle(mangled_name, 0, 0, &status);
        if (cc) {
            _snprintf(output_buf, avail_len, cur_len, msg_len, "%s at ", cc);
        } else {
            std::string msg_str = stack_msg[i];
            std::string _func_name = msg_str;
            size_t s_pos = msg_str.find("(");
            size_t e_pos = msg_str.rfind("+");
            if (e_pos == std::string::npos) e_pos = msg_str.rfind(")");
            if ( s_pos != std::string::npos &&
                 e_pos != std::string::npos ) {
                _func_name = msg_str.substr(s_pos+1, e_pos-s_pos-1);
            }
            _snprintf(output_buf, avail_len, cur_len, msg_len,
                      "%s() at ", _func_name.c_str() );
        }

        _snprintf(output_buf, avail_len, cur_len, msg_len, "%s\n", file_line);

#else
        // On non-Linux platform, just use the raw symbols.
        size_t msg_len = 0;
        size_t avail_len = output_buflen;
        _snprintf(output_buf, avail_len, cur_len, msg_len, "%s\n", stack_msg[i]);

#endif
    }
    free(stack_msg);

    return cur_len;
}

static SIZE_T_UNUSED
stack_backtrace(char* output_buf, size_t output_buflen) {
    void* stack_ptr[256];
    int stack_size = _stack_backtrace(stack_ptr, 256);
    return _stack_interpret(stack_ptr, stack_size, output_buf, output_buflen);
}


// LCOV_EXCL_STOP

