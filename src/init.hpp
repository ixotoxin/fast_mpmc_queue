// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#ifndef _DEBUG
#   undef ENABLE_MEMORY_PROFILING
#endif

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       error Requires WIN32_LEAN_AND_MEAN definition
#   endif
#   ifndef NOMINMAX
#       error Requires NOMINMAX definition
#   endif
#   include <windows.h>
#   include <io.h>
#   include <fcntl.h>
#   include <clocale>
#   if defined(ENABLE_MEMORY_PROFILING) && defined(_DEBUG) && (defined(_MSC_VER) || defined(__clang__))
#       include <crtdbg.h>
#       include <iostream>
#   else
#       undef ENABLE_MEMORY_PROFILING
#   endif
#else
#   undef ENABLE_MEMORY_PROFILING
#endif

namespace init {
    inline void console() {
#ifdef _WIN32
        ::SetConsoleOutputCP(CP_UTF8);
        ::SetConsoleCP(CP_UTF8);
        ::setlocale(LC_ALL, ".UTF8");
#endif
    }

    inline void profiler() {
#ifdef ENABLE_MEMORY_PROFILING
        std::cout << "!! Enabled memory profiling !!" << std::endl;
        constexpr auto report_mode = /*_CRTDBG_MODE_DEBUG |*/ _CRTDBG_MODE_FILE /*| _CRTDBG_MODE_WNDW*/;
        ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
        ::_CrtSetReportMode(_CRT_ASSERT, report_mode);
        ::_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        ::_CrtSetReportMode(_CRT_WARN, report_mode);
        ::_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        ::_CrtSetReportMode(_CRT_ERROR, report_mode);
        ::_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    }
}
