#include <Windows.h>
#include "system/Input.h"

bool Input::IsKeyPressed(VirtualKey key)
{
	return GetKeyState((int)key) & 0x8000;
}

bool Input::IsKeyToggled(VirtualKey key)
{
	return GetKeyState((int)key) & 1;
}

bool Input::GetGlobalCursorPosition(int& x, int& y)
{
	POINT point;
	bool ret = GetCursorPos(&point);
	x = point.x;
	y = point.y;
	return ret;
}