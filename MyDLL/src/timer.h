#pragma once
#include "Windows.h"

struct timer_t
{
    LARGE_INTEGER StartingTime;
    LARGE_INTEGER EndingTime;
};

void TimerStart(timer_t& time);

void TimerStop(timer_t& time);

// Return elapsed time in microseconds.
LARGE_INTEGER TimerElapsed(timer_t& time);
