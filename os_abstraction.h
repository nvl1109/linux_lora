#ifndef ___OS_ABSTRACTION_H___
#define ___OS_ABSTRACTION_H___ 1

#include <time.h>

static inline void os_delay_ms(int ms) {
    struct timespec ts = {
        .tv_sec = ms/1000,
        .tv_nsec = (ms % 1000) * 1000000,
    };

    nanosleep(&ts, NULL);
}

#endif /* ___OS_ABSTRACTION_H___ */
