
#include "InputHandler.h"

using namespace std;
using namespace Speck;

InputHandler::InputHandler()
	: mHwnd(0)
{

}

void InputHandler::Update(float dt)
{
	// Keyboard
	map<UINT, float>::iterator ite = mCurrKeyboardState.begin();

	while (ite != mCurrKeyboardState.end())
	{
		ite->second += dt;
		ite++;
	}

	// Mouse
	mPrevMouseState = mCurrMouseState;
	mCurrMouseState = mNextMouseState;
}

void InputHandler::GetMouseStates(MouseState *curr, MouseState *prev) const
{
	*curr = mCurrMouseState;
	*prev = mPrevMouseState;
}

void InputHandler::GetMouseCoords(int *sx, int *sy) const
{
	*sx = mCurrMouseState.x;
	*sy = mCurrMouseState.y;
}

bool InputHandler::IsPressed(UINT keyCode, float *pressedTime) const
{
	map<UINT, float>::const_iterator ite;
	for (ite = mCurrKeyboardState.begin(); ite != mCurrKeyboardState.end(); ite++)
	{
		if (ite->first == keyCode)
			break;
	}

	*pressedTime = 0;
	if (ite == mCurrKeyboardState.end()) // There is no such key in this map.
		return false;
	else
	{
		*pressedTime = ite->second;
		return true;
	}
}

void InputHandler::Win32MsgProc(MSG *msg)
{
	switch (msg->message)
	{
	case WM_KEYDOWN:
		if (mCurrKeyboardState.find((UINT)msg->wParam) == mCurrKeyboardState.end()) // There is no such key in this map.
			mCurrKeyboardState.insert(pair<UINT, float>((UINT)msg->wParam, 0.0f));
		return;
	case WM_KEYUP:
		mCurrKeyboardState.erase((UINT)msg->wParam);
		return;
	case WM_MOUSEWHEEL:
		{
			mNextMouseState.wheelValue += GET_WHEEL_DELTA_WPARAM(msg->wParam) / WHEEL_DELTA;
		}
		return;
	}

	if (msg->hwnd == mHwnd)
	{
		switch (msg->message)
		{
		case WM_LBUTTONDOWN:
			mNextMouseState.IsLMBDown = true;
			return;
		case WM_MBUTTONDOWN:
			mNextMouseState.IsMMBDown = true;
			return;
		case WM_RBUTTONDOWN:
			mNextMouseState.IsRMBDown = true;
			return;
		case WM_LBUTTONUP:
			mNextMouseState.IsLMBDown = false;
			return;
		case WM_MBUTTONUP:
			mNextMouseState.IsMMBDown = false;
			return;
		case WM_RBUTTONUP:		
			mNextMouseState.IsRMBDown = false;
			return;
		case WM_MOUSEMOVE:
			{
				mNextMouseState.x = GET_X_LPARAM(msg->lParam);
				mNextMouseState.y = GET_Y_LPARAM(msg->lParam);
			}
		default:
			{
				//Check the mouse left button is pressed or not
				if ((GetKeyState(VK_LBUTTON) & 0x80) != 0)
					mNextMouseState.IsLMBDown = true;
				else
					mNextMouseState.IsLMBDown = false;

				//Check the mouse right button is pressed or not
				if ((GetKeyState(VK_RBUTTON) & 0x80) != 0)
					mNextMouseState.IsRMBDown = true;
				else
					mNextMouseState.IsRMBDown = false;

				//Check the mouse middle button is pressed or not
				if ((GetKeyState(VK_MBUTTON) & 0x80) != 0)
					mNextMouseState.IsMMBDown = true;
				else
					mNextMouseState.IsMMBDown = false;
			}
			return;
		}
	}
}
