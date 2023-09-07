#include "system/Window.h"
#include <windowsx.h>
#include <iostream>
#include <stdexcept>
#include "system/SystemMetrics.h"
#include "Console.h"

std::map<HWND, Win32Window*> Win32Window::windowBinding;
std::vector<Win32Window*> Win32Window::windows;

MSG Win32Window::message;

Win32Window::Win32Window(const Win32WindowCreateInfo& createInfo)
{;
	width = createInfo.width;
	height = createInfo.height;
	x = createInfo.x;
	y = createInfo.y;
	className = createInfo.className;
	currentWindowMode = createInfo.windowMode;

	hInstance = GetModuleHandle(NULL);

	WNDCLASSEX wc{};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = createInfo.icon;
	wc.hCursor = createInfo.cursor;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = createInfo.className.c_str();
	wc.hIconSm = createInfo.icon;

	if (!RegisterClassExW(&wc))
		throw std::runtime_error("Failed to register a window: " + GetLastErrorAsString());
	
	if (createInfo.windowMode == WINDOW_MODE_BORDERLESS_WINDOWED)
	{
		width = GetSystemMetrics(SM_CXSCREEN);
		height = GetSystemMetrics(SM_CYSCREEN);
		window = CreateWindowExW((DWORD)createInfo.extendedWindowStyle, createInfo.className.c_str(), createInfo.windowName.c_str(), (DWORD)WindowStyle::PopUp, createInfo.x, createInfo.y, monitorWidth, monitorHeight, NULL, NULL, hInstance, NULL);
	}
	else
		window = CreateWindowExW((DWORD)createInfo.extendedWindowStyle, createInfo.className.c_str(), createInfo.windowName.c_str(), (DWORD)createInfo.style, createInfo.x, createInfo.y, createInfo.width, createInfo.height, NULL, NULL, hInstance, NULL);

	windowBinding[window] = this;
	windows.push_back(this);
	
	if (window == NULL)
		throw std::runtime_error("Failed to create a window: " + GetLastErrorAsString());

	ShowWindow(window, SW_NORMAL);
	UpdateWindow(window);

	SetTimer(window, 0, USER_TIMER_MINIMUM, NULL); //makes sure the window is being updated even when if the cursor isnt moving

	RAWINPUTDEVICE rawInputDevice{};
	rawInputDevice.usUsagePage = 0x01;
	rawInputDevice.usUsage = 0x02;
	rawInputDevice.dwFlags = 0;
	rawInputDevice.hwndTarget = 0;

	if (!RegisterRawInputDevices(&rawInputDevice, 1, sizeof(rawInputDevice)))
		throw std::runtime_error("Failed to register the raw input device for the mouse");
}

void Win32Window::PollEvents()
{
	for (Win32Window* w : windows)
	{
		w->cursorX = 0;
		w->cursorY = 0;
		w->allMessagesFromLastPoll.clear();
	}
	while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) 
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}

void Win32Window::ChangeWindowStyle(WindowStyle newWindowStyle)
{
	SetWindowLongPtr(window, GWL_STYLE, (DWORD)newWindowStyle);
	SetWindowPos(window, 0, x, y, width, height, SWP_SHOWWINDOW);
}

WindowMode Win32Window::GetWindowMode()
{
	return currentWindowMode;
}

bool Win32Window::ShouldClose()
{
	return shouldClose;
}

int Win32Window::GetWidth()
{
	return width;
}

int Win32Window::GetHeight()
{
	return height;
}

void Win32Window::GetWindowDimensions(int* width, int* height)
{
	*width = this->width;
	*height = this->height;
}

void Win32Window::GetRelativeCursorPosition(int& x, int& y)
{
	x = cursorX;
	y = cursorY;
}

int Win32Window::GetWheelRotation()
{
	return wheelRotation;
}

void Win32Window::LockCursor()
{
	ShowCursor(false);
	lockCursor = true;
}

void Win32Window::UnlockCursor()
{
	ShowCursor(true);
	lockCursor = false;
}

bool Win32Window::CursorIsLocked()
{
	return lockCursor;
}

bool Win32Window::ContainsDroppedFile()
{
	return containsDroppedFile;
}

std::string Win32Window::GetDroppedFile()
{
	containsDroppedFile = false;
	return droppedFile;
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (windowBinding.count(hwnd) == 0)
		return DefWindowProc(hwnd, message, wParam, lParam);

	if (windowBinding[hwnd]->additionalPollCallback != nullptr)
		windowBinding[hwnd]->additionalPollCallback(hwnd, message, wParam, lParam);

	windowBinding[hwnd]->allMessagesFromLastPoll.push_back(message);

	switch (message)
	{
		case WM_CREATE:
			break;
		case WM_CLOSE:
			windowBinding[hwnd]->shouldClose = true;
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_SIZE:
		{
			windowBinding[hwnd]->resized = true;
			windowBinding[hwnd]->width = LOWORD(lParam);
			windowBinding[hwnd]->height = HIWORD(lParam);
		}
			break;
		case WM_MOUSEMOVE:
			if (windowBinding[hwnd]->lockCursor)
			{
				ShowCursor(false);
				SetCursorPos(windowBinding[hwnd]->width / 2, windowBinding[hwnd]->height / 2);
			}
			else
				ShowCursor(true);
			break;
		case WM_MOUSEWHEEL:
			windowBinding[hwnd]->wheelRotation = GET_WHEEL_DELTA_WPARAM(wParam);
			break;
		case WM_INPUT:
		{
			UINT dwSize = sizeof(RAWINPUT);

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
			LPBYTE lpb = new BYTE[dwSize];
			if (lpb == NULL)
				return 0;

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
				throw std::runtime_error("GetRawInputData does not return correct size");

			RAWINPUT* rawInput = (RAWINPUT*)lpb;
			if (rawInput->header.dwType == RIM_TYPEMOUSE)
			{
				windowBinding[hwnd]->cursorX = rawInput->data.mouse.lLastX;
				windowBinding[hwnd]->cursorY = rawInput->data.mouse.lLastY;
			}
			else
			{
				windowBinding[hwnd]->cursorX = 0;
				windowBinding[hwnd]->cursorY = 0;
			}
			delete[] lpb;
			break;
		}
		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;
			UINT bufferSize = DragQueryFileA(hDrop, 0, NULL, 512); // DragQueryFileA returns the length of the path of the file if the pointer to the file buffer is NULL
			char* fileBuffer = new char[bufferSize + 1]; // + 1 to account for the null terminator
			DragQueryFileA(hDrop, 0, fileBuffer, bufferSize + 1);
			
			windowBinding[hwnd]->containsDroppedFile = true;
			windowBinding[hwnd]->droppedFile = std::string(fileBuffer);

#ifdef _DEBUG
			std::cout << "Detected dropped file: " + windowBinding[hwnd]->droppedFile << std::endl;
#endif

			delete[] fileBuffer;
			break;
		}
	
		case WM_TIMER:
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
	}
}

void Win32Window::Destroy()
{
	delete this;
}

Win32Window::~Win32Window()
{
	DestroyWindow(window);
	UnregisterClassW(className.c_str(), hInstance);
}