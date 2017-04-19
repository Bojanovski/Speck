
#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	struct MouseState
	{
		bool IsLMBDown;
		bool IsMMBDown;
		bool IsRMBDown;

		// Coordinates
		int x;
		int y;

		// Wheel
		int wheelValue;

		// This constructor will be called automatically
		MouseState()
		{
			IsLMBDown = IsMMBDown = IsRMBDown = false;
			wheelValue = x = y = 0;
		}
	};

	class InputHandler
	{
		friend class D3DApp;

	public:
		InputHandler();
		~InputHandler() {}

		void Initialize(HWND hwnd) { mHwnd = hwnd; }
		void GetMouseStates(MouseState *curr, MouseState *prev) const;
		void GetMouseCoords(int *sx, int *sy) const;
		bool IsPressed(UINT keyCode, float *pressedTime) const;

	private:
		MouseState mPrevMouseState, mCurrMouseState, mNextMouseState;
		HWND mHwnd;

		// Virtual key codes http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx
		std::map<UINT, float> mCurrKeyboardState;

		void Update(float dt);
		void Win32MsgProc(MSG *msg);
	};
}

#endif
