#ifndef GAMESERVER_COMMON_ASSERT_H
#define GAMESERVER_COMMON_ASSERT_H

#include <cstdlib>

#if !OS_WINDOWS && !OS_LINUX
#   if defined(_WIN32)
#       define OS_WINDOWS 1
#   elif defined(__linux__) || defined(__gnu_linux__)
#       define OS_LINUX 1
#   else
#       error "Operating system not supported."
#   endif
#endif

#if !COMPILER_MSVC && !COMPILER_GCC && !COMPILER_CLANG
#   if defined(_MSC_VER)
#       define COMPILER_MSVC 1
#   elif defined(__GNUC__)
#       define COMPILER_GCC 1
#   elif defined(__clang__)
#       define COMPILER_CLANG 1
#   endif
#endif

#if COMPILER_GCC || COMPILER_CLANG
#   define ATTR_FALLTHROUGH __attribute__((fallthrough))
#   define ATTR_PRINTF(x, y) __attribute__((format(printf, x, y)))
#else
#   define ATTR_FALLTHROUGH
#   define ATTR_PRINTF(x, y)
#endif

#if COMPILER_MSVC
#   define TRAP() __debugbreak()
#elif COMPILER_GCC || COMPILER_CLANG
#   define TRAP() __builtin_trap()
#else
#   define TRAP() abort()
#endif

#define ASSERT_ALWAYS(expr) do { if(!(expr)) { TRAP(); } } while(0)
#if ENABLE_ASSERTIONS
#   define ASSERT(expr) ASSERT_ALWAYS(expr)
#else
#   define ASSERT(expr) ((void)(expr))
#endif

static_assert(OS_LINUX, "Server requires Linux");

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>

// NOTE(fusion): This is the member name for the thread id in `struct sigevent`
// when `sigev_notify` is `SIGEV_THREAD_ID` but for whatever reason glibc doesn't
// define it. musl defines it in <signal.h> so the include above covers that case.
#ifndef sigev_notify_thread_id
#   define sigev_notify_thread_id _sigev_un._tid
#endif

// musl doesn't expose tgkill(); use the syscall directly.
#ifndef __GLIBC__
static inline int tgkill(int tgid, int tid, int sig) {
    return syscall(SYS_tgkill, tgid, tid, sig);
}
#endif

#endif // GAMESERVER_COMMON_ASSERT_H
