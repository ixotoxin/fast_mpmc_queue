// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER) || defined (__INTEL_LLVM_COMPILER)
#   define EXECUTE_BEFORE_MAIN(FUNC) static void __attribute__((constructor)) FUNC(void)
#else
#   define EXECUTE_BEFORE_MAIN(FUNC) \
        inline void FUNC##___f(void); \
        struct FUNC##___t { FUNC##___t(void) { FUNC##___f(); } }; \
        static FUNC##___t FUNC##___s; \
        inline void FUNC##___f(void)
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
EXECUTE_BEFORE_MAIN(set_console_output_cp) {
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
    ::setlocale(LC_ALL, ".UTF8");
    // ::SetConsoleOutputCP(CP_ACP);
    // ::SetConsoleCP(CP_ACP);
    // ::setlocale(LC_ALL, ".ACP");
}
#   if defined(ENABLE_MEMORY_PROFILING) && defined(_DEBUG) && (defined(_MSC_VER) || defined(__clang__))
#       include <iostream>
#       include <crtdbg.h>
EXECUTE_BEFORE_MAIN(enable_memory_profiling) {
    std::cout << "!! Enabled memory profiling !!" << std::endl;
    constexpr auto report_mode = /*_CRTDBG_MODE_DEBUG |*/ _CRTDBG_MODE_FILE /*| _CRTDBG_MODE_WNDW*/;
    ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
    ::_CrtSetReportMode(_CRT_ASSERT, report_mode);
    ::_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    ::_CrtSetReportMode(_CRT_WARN, report_mode);
    ::_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    ::_CrtSetReportMode(_CRT_ERROR, report_mode);
    ::_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
}
#   endif
#endif
