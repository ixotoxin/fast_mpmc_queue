// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <crtdbg.h>
#include "execute_before_main.hpp"

EXECUTE_BEFORE_MAIN(enable_memory_profiling) {
    constexpr auto report_mode = /*_CRTDBG_MODE_DEBUG |*/ _CRTDBG_MODE_FILE /*| _CRTDBG_MODE_WNDW*/;
    ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
    ::_CrtSetReportMode(_CRT_ASSERT, report_mode);
    ::_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    ::_CrtSetReportMode(_CRT_WARN, report_mode);
    ::_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    ::_CrtSetReportMode(_CRT_ERROR, report_mode);
    ::_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
}
