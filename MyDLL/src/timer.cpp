#include "pch.h"
#include "timer.h"

void TimerStart(timer_t& time)
{
    QueryPerformanceCounter(&time.StartingTime);
}

void TimerStop(timer_t& time)
{
    QueryPerformanceCounter(&time.EndingTime);
}

// Return elapsed time in microseconds.
LARGE_INTEGER TimerElapsed(timer_t& time)
{
    LARGE_INTEGER ElapsedMicroseconds;
    LARGE_INTEGER Frequency;

    QueryPerformanceFrequency(&Frequency);
    ElapsedMicroseconds.QuadPart = time.EndingTime.QuadPart - time.StartingTime.QuadPart;

    // Convert to microseconds.
    ElapsedMicroseconds.QuadPart *= 1000000;
    ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

    return ElapsedMicroseconds;
}