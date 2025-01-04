#include <Windows.h>
#include "system/Input.h"

struct KeyboardState
{
public:
	bool Fetch()
	{
		for (int i = 0; i < STATE_SIZE; i++)
			state[i] = GetKeyState(i);

		return true;//GetKeyboardState(state);
	}

	void CopyFrom(const KeyboardState& ks)
	{
		memcpy(state, ks.state, STATE_SIZE * sizeof(SHORT));
	}

	SHORT& operator[](VirtualKey key)
	{
		return state[static_cast<int>(key)];
	}
 
private:
	static constexpr unsigned int STATE_SIZE = 256U;

	SHORT state[STATE_SIZE]{ 0 };
};

KeyboardState currState, prevState;

bool Input::FetchState()
{
	prevState.CopyFrom(currState);
	return currState.Fetch();
}

bool Input::IsKeyJustReleased(VirtualKey key)
{
	bool prev = prevState[key] & (1 << 15);
	bool curr = currState[key] & (1 << 15);

	return prev && !curr;
}

bool Input::IsKeyJustPressed(VirtualKey key)
{
	bool prev = prevState[key] & (1 << 15);
	bool curr = currState[key] & (1 << 15);

	return curr && !prev;
}

bool Input::IsKeyPressed(VirtualKey key)
{
	return currState[key] & (1 << 15);
}

bool Input::IsKeyToggled(VirtualKey key)
{
	return currState[key] & (1 << 0);
}

bool Input::GetGlobalCursorPosition(int& x, int& y)
{
	POINT point;
	bool ret = GetCursorPos(&point);
	x = point.x;
	y = point.y;
	return ret;
}