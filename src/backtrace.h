/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Stack Backtrace
 * Version: 0.1.3
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

#define SIZE_T_UNUSED size_t __attribute__((unused))

// Only on Linux.
#ifdef __linux__

#include <cstddef>
#include <string>

#include <cxxabi.h>
#include <execinfo.h>
#include <inttypes.h>
#include <stdio.h>
#include <signal.h>

static SIZE_T_UNUSED
stack_backtrace(char* output_buf, size_t output_buflen) {
    void* stack_ptr[256];
    int stack_size = 0;
    stack_size = backtrace(stack_ptr, 256);

    char** stack_msg = nullptr;
    stack_msg = backtrace_symbols(stack_ptr, stack_size);

    size_t offset = 0;
    size_t frame_num = 0;
    for (int i=0; i<stack_size; ++i) {
        int fname_len = 0;
        while ( stack_msg[i][fname_len] != '(' &&
                stack_msg[i][fname_len] != ' ' &&
                stack_msg[i][fname_len] != 0 ) {
            ++fname_len;
        }

        char cmd[1024];
        sprintf( cmd, "addr2line -f -e %.*s %" PRIxPTR "",
                 fname_len, stack_msg[i], (uintptr_t)stack_ptr[i] );
        FILE* fp = popen(cmd, "r");
        if (!fp) continue;

        char mangled_name[1024];
        char file_line[1024];
        int ret = fscanf(fp, "%1023s %1023s", mangled_name, file_line);
        (void)ret;
        pclose(fp);

        offset += snprintf( output_buf + offset, output_buflen - offset,
                            "#%-2zu 0x%016" PRIxPTR " in ",
                            frame_num++, (uintptr_t)stack_ptr[i] );
        if (offset >= output_buflen) return output_buflen;

        int status;
        char *cc = abi::__cxa_demangle(mangled_name, 0, 0, &status);
        if (cc) {
            offset += snprintf( output_buf + offset, output_buflen - offset,
                                "%s at ", cc );
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
            offset += snprintf( output_buf + offset, output_buflen - offset,
                                "%s() at ", _func_name.c_str() );
        }
        if (offset >= output_buflen) return output_buflen;

        offset += snprintf( output_buf + offset, output_buflen - offset,
                            "%s\n", file_line );
        if (offset >= output_buflen) return output_buflen;
    }

    return offset;
}

#else

static SIZE_T_UNUSED
stack_backtrace(char* output_buf, size_t output_buflen) {
    return 0;
}

#endif
