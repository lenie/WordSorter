#ifndef __MyTimer_H__
#define __MyTimer_H__
#include <windows.h>

class MyTimer
{
private:
    LARGE_INTEGER  _freq;
    LARGE_INTEGER  _begin;
    LARGE_INTEGER  _end;

public:
    double d_costTime;

public:
    MyTimer()
    {
        QueryPerformanceFrequency(&_freq);//get counter(cpu)'s frequence
        d_costTime = 0; 
    }

    void Start()
    {
        QueryPerformanceCounter(&_begin);
    }

    void Stop()
    {
        QueryPerformanceCounter(&_end);
        d_costTime = (double)((_end.QuadPart - _begin.QuadPart) * 1000 / _freq.QuadPart); //calculate time(unit: ms)
    }

    void Reset()
    {
        d_costTime = 0;
    }
};
#endif
