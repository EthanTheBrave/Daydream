#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// --- Timing ---
#ifdef _WIN32
#  include <windows.h>
inline uint32_t millis() { return GetTickCount(); }
inline void delay(uint32_t ms) { Sleep(ms); }
#else
#  include <time.h>
inline uint32_t millis() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec*1000 + ts.tv_nsec/1000000);
}
inline void delay(uint32_t ms) {
    struct timespec ts = { (time_t)(ms/1000), (long)(ms%1000)*1000000L };
    nanosleep(&ts, nullptr);
}
#endif

// --- Math (avoid std:: conflicts) ---
#undef min
#undef max
#undef abs
#define min(a,b)          ((a)<(b)?(a):(b))
#define max(a,b)          ((a)>(b)?(a):(b))
#define abs(x)            ((x)<0?(-(x)):(x))
#define constrain(x,lo,hi) ((x)<(lo)?(lo):(x)>(hi)?(hi):(x))

// --- Random ---
inline void    randomSeed(uint32_t s)        { srand(s); }
inline int32_t random(int32_t hi)            { return (int32_t)(rand() % hi); }
inline int32_t random(int32_t lo, int32_t hi){ if(hi<=lo) return lo; return lo + (int32_t)(rand()%(hi-lo)); }

// --- Serial ---
struct _Serial {
    void begin(int) {}
    void print(const char* s)    { fputs(s, stderr); }
    void print(char c)           { fputc(c, stderr); }
    void print(int v)            { fprintf(stderr, "%d", v); }
    void print(unsigned long v)  { fprintf(stderr, "%lu", v); }
    void println(const char* s)  { fprintf(stderr, "%s\n", s); }
    void println(int v)          { fprintf(stderr, "%d\n", v); }
    void println()               { fputc('\n', stderr); }
};
static _Serial Serial;

// --- Pins (no-op) ---
#define OUTPUT 1
#define INPUT  0
#define HIGH   1
#define LOW    0
#define TFT_BL 21
inline void pinMode(int,int)      {}
inline void digitalWrite(int,int) {}
