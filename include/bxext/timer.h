#pragma once

#include "../bx/timer.h"

namespace bx 
{
    class Timer
    {
    public:
        Timer();

        void start();
        double read();
        double readDelta();

    private:
        int64_t m_freq;
        int64_t m_last;
        int64_t m_start;
        double m_toMs;
    };

    //
    inline Timer::Timer()
    {
        m_freq = bx::getHPFrequency();
        m_toMs = 1000.0/(double)m_freq;
        m_last = 0;
        m_start = 0;
    }

    inline void Timer::start()
    {
        m_start = bx::getHPCounter();
        m_last = m_start;
    }

    inline double Timer::read()
    {
        int64_t now = bx::getHPCounter();
        return double(now - m_start)*m_toMs;
    }

    inline double Timer::readDelta()
    {
        int64_t now = bx::getHPCounter();
        double d = double(now - m_last)*m_toMs;
        m_last = now;
        return d;
    }
}