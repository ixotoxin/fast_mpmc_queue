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
