#ifndef GAME_TIMER_H
#define GAME_TIMER_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	class GameTimer
	{
	public:
		__declspec(dllexport) GameTimer();

		__declspec(dllexport) float TotalTime()const;  // in seconds
		__declspec(dllexport) float DeltaTime()const; // in seconds

		__declspec(dllexport) void Reset(); // Call before message loop.
		__declspec(dllexport) void Start(); // Call when unpaused.
		__declspec(dllexport) void Stop();  // Call when paused.
		__declspec(dllexport) void Tick();  // Call every frame.

		// In seconds.
		void SetMinFrameTime(double minFrameTime) { xMinFrameTime = minFrameTime; }
		// In seconds.
		double GetMinFrameTime() const { return xMinFrameTime; }

	private:
		double xSecondsPerCount;
		double xDeltaTime;
		double xMinFrameTime;

		__int64 xBaseTime;
		__int64 xPausedTime;
		__int64 xStopTime;
		__int64 xPrevTime;
		__int64 xCurrTime;

		bool xStopped;
	};
}

#endif
