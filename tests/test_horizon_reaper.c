#include "test_framework.h"
#include "../src/daemon/horizon_reaper.h"
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

TEST(test_pid_alive_self) {
#ifdef _WIN32
    uint32_t my_pid = (uint32_t)GetCurrentProcessId();
#else
    uint32_t my_pid = (uint32_t)getpid();
#endif
    ASSERT_TRUE(cbm_is_pid_alive(my_pid));
    PASS();
}

TEST(test_pid_dead_invalid) {
    /* Large non-existent PID */
    ASSERT_FALSE(cbm_is_pid_alive(99999999));
    PASS();
}

SUITE(horizon_reaper_suite) {
    RUN_TEST(test_pid_alive_self);
    RUN_TEST(test_pid_dead_invalid);
}
