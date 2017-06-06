#include "Timer.h"

using namespace Speck;

Timer::Timer()
	: xSecondsPerCount(0), 
	xDeltaTime(-1.0), 
	xMinFrameTime(0),
	xBaseTime(0), 
	xPausedTime(0),
	xPrevTime(0), 
	xCurrTime(0), 
	xStopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER *)&countsPerSec);
	xSecondsPerCount = 1.0 / (double)countsPerSec;
}

// Returns the total time elapsed since Reset() was called, NOT counting any
// time when the clock is stopped.
float Timer::TotalTime()const
{
	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// mStopTime - mBaseTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from mStopTime:  
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime

	if( xStopped )
	{
		return (float)(((xStopTime - xPausedTime)-xBaseTime)*xSecondsPerCount);
	}

	// The distance mCurrTime - mBaseTime includes paused time,
	// which we do not want to count.  To correct this, we can subtract 
	// the paused time from mCurrTime:  
	//
	//  (mCurrTime - mPausedTime) - mBaseTime 
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mCurrTime
	
	else
	{
		return (float)(((xCurrTime-xPausedTime)-xBaseTime)*xSecondsPerCount);
	}
}

float Timer::DeltaTime()const
{
	return (float)xDeltaTime;
}

void Timer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	xBaseTime = currTime;
	xPrevTime = currTime;
	xStopTime = 0;
	xStopped  = false;
}

void Timer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);


	// Accumulate the time elapsed between stop and start pairs.
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  mBaseTime       mStopTime        startTime     

	if( xStopped )
	{
		xPausedTime += (startTime - xStopTime);	

		xPrevTime = startTime;
		xStopTime = 0;
		xStopped  = false;
	}
}

void Timer::Stop()
{
	if( !xStopped )
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		xStopTime = currTime;
		xStopped  = true;
	}
}

void Timer::Tick()
{
	if( xStopped )
	{
		xDeltaTime = 0.0;
		return;
	}

	do
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		xCurrTime = currTime;

		// Time difference between this frame and the previous.
		xDeltaTime = (xCurrTime - xPrevTime)*xSecondsPerCount;
	} while (xDeltaTime < xMinFrameTime);

	// Prepare for next frame.
	xPrevTime = xCurrTime;

	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then mDeltaTime can be negative.
	if(xDeltaTime < 0.0)
	{
		xDeltaTime = 0.0;
	}
}
