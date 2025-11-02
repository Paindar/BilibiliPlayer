#pragma once

// Lightweight thread-safety annotation macros for documentation and future
// static analysis. They are no-ops on compilers that don't support the
// attributes (default here).
#if defined(__clang__)
# define TS_GUARDED_BY(x) __attribute__((guarded_by(x)))
# define TS_REQUIRES(...) __attribute__((exclusive_locks_required(__VA_ARGS__)))
# define TS_EXCLUSIVE_LOCKS_REQUIRED(...) __attribute__((exclusive_locks_required(__VA_ARGS__)))
#else
# define TS_GUARDED_BY(x)
# define TS_REQUIRES(...)
# define TS_EXCLUSIVE_LOCKS_REQUIRED(...)
#endif
