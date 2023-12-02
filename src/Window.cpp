#include "system/Window.h"
#include <windowsx.h>
#include <iostream>
#include <stdexcept>

std::map<HWND, Win32Window*> Win32Window::windowBinding;
std::unordered_set<Win32Window*> Win32Window::windows;

MSG Win32Window::message;

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string WinGetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return std::string(); //No error message has been recorded
	}

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}

Win32Window::Win32Window(const Win32WindowCreateInfo& createInfo)
{
	width = createInfo.width;
	height = createInfo.height;
	x = createInfo.x;
	y = createInfo.y;
	className = createInfo.className;
	windowName = createInfo.windowName;
	currentWindowMode = createInfo.windowMode;
	extendedWindowStyle = createInfo.extendedWindowStyle;
	icon = createInfo.icon;
	cursor = createInfo.cursor;

	hInstance = GetModuleHandle(NULL);

	WNDCLASSEX wc{};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = icon;
	wc.hCursor = cursor;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = className.c_str();
	wc.hIconSm = icon;

	if (!RegisterClassExW(&wc))
		throw std::runtime_error("Failed to register a window: " + WinGetLastErrorAsString());
	
	if (createInfo.windowMode == WINDOW_MODE_BORDERLESS_WINDOWED)
	{
		width = GetSystemMetrics(SM_CXSCREEN);
		height = GetSystemMetrics(SM_CYSCREEN);
		window = CreateWindowExW(WS_EX_APPWINDOW, className.c_str(), windowName.c_str(), WS_POPUPWINDOW, createInfo.x, createInfo.y, monitorWidth, monitorHeight, NULL, NULL, hInstance, NULL);
	}
	else if (createInfo.windowMode == WINDOW_MODE_WINDOWED)
		window = CreateWindowExW((DWORD)createInfo.extendedWindowStyle, className.c_str(), windowName.c_str(), (DWORD)WindowStyle::OverlappedWindow, createInfo.x, createInfo.y, createInfo.width, createInfo.height, NULL, NULL, hInstance, NULL);

	else
		window = CreateWindowExW((DWORD)createInfo.extendedWindowStyle, className.c_str(), windowName.c_str(), (DWORD)createInfo.style, createInfo.x, createInfo.y, createInfo.width, createInfo.height, NULL, NULL, hInstance, NULL);

	windowBinding[window] = this;
	windows.insert(this);
	
	if (window == NULL)
		throw std::runtime_error("Failed to create a window: " + WinGetLastErrorAsString());

	maximized = createInfo.startMaximized;
	ShowWindow(window, maximized);
	UpdateWindow(window);

	SetTimer(window, 0, USER_TIMER_MINIMUM, NULL); //makes sure the window is being updated even when if the cursor isnt moving

	RAWINPUTDEVICE rawInputDevice{};
	rawInputDevice.usUsagePage = 0x01;
	rawInputDevice.usUsage = 0x02;
	rawInputDevice.dwFlags = 0; // no flags
	rawInputDevice.hwndTarget = NULL;

	if (!RegisterRawInputDevices(&rawInputDevice, 1, sizeof(rawInputDevice)))
		throw std::runtime_error("Failed to register the raw input device for the mouse");

#ifdef _DEBUG
	std::string message = "Created new window with dimensions " + std::to_string(width) + 'x' + std::to_string(height) + " and mode " + WindowModeToString(currentWindowMode);
	std::cout << message << std::endl;
#endif
}

void Win32Window::ChangeWindowMode(WindowMode windowMode)
{
	if (windowMode == currentWindowMode)
		return;

	if (currentWindowMode == WINDOW_MODE_BORDERLESS_WINDOWED)
	{
		width = monitorWidth / 2;
		height = monitorHeight / 2;
		SetWindowLongPtr(window, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		if (SetWindowPos(window, NULL, width / 2, height / 2, width, height, SWP_FRAMECHANGED) == 0)
			throw std::runtime_error("Failed to resize the window to windowed mode: " + WinGetLastErrorAsString());
		currentWindowMode = WINDOW_MODE_WINDOWED;
	}
	else if (currentWindowMode == WINDOW_MODE_WINDOWED)
	{
		width = monitorWidth;
		height = monitorHeight;
		SetWindowLong(window, GWL_STYLE, WS_POPUPWINDOW);
		if (SetWindowPos(window, NULL, 0, 0, monitorWidth, monitorHeight, SWP_FRAMECHANGED) == 0)
			throw std::runtime_error("Failed to resize the window to borderless windowed mode: " + WinGetLastErrorAsString());
		currentWindowMode = WINDOW_MODE_BORDERLESS_WINDOWED;
	}
}

void Win32Window::PollMessages()
{
	for (Win32Window* w : windows)
	{
		w->cursorX = 0;
		w->cursorY = 0;
		w->wheelRotation = 0;

		ShowWindow(w->window, w->maximized); // dont know if this call is expensive to make every frame
	}
	while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) 
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
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

void Win32Window::GetRelativeCursorPosition(int& x, int& y)
{
	x = cursorX;
	y = cursorY;
}

void Win32Window::GetAbsoluteCursorPosition(int& x, int& y)
{
	x = absCursorX;
	y = absCursorY;
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

	switch (message)
	{
		case WM_CREATE: // when the window is created
			break;
		case WM_CLOSE: // when the window should close
			windowBinding[hwnd]->shouldClose = true;
			DestroyWindow(hwnd);
			break;

		case WM_DESTROY: // when the window is getting destroyed
			PostQuitMessage(0);
			break;

		case WM_SIZE: // when the window is resized
		{
			RECT resizedWindowDimensions;
			GetWindowRect(hwnd, &resizedWindowDimensions);

			windowBinding[hwnd]->resized = true;
			windowBinding[hwnd]->width = resizedWindowDimensions.right - resizedWindowDimensions.left;
			windowBinding[hwnd]->height = resizedWindowDimensions.bottom - resizedWindowDimensions.top;
			break;
		}

		case WM_MOUSEMOVE: // when the cursor has moved
			if (windowBinding[hwnd]->lockCursor) // if the cursor is locked it has to stay inside the window, so this locks resets it to the center of the screen
				SetCursorPos(windowBinding[hwnd]->width / 2, windowBinding[hwnd]->height / 2);
			windowBinding[hwnd]->absCursorX = GET_X_LPARAM(lParam);
			windowBinding[hwnd]->absCursorY = GET_Y_LPARAM(lParam);
			break;

		case WM_MOUSEWHEEL: // when the mouse wheel has moved
			windowBinding[hwnd]->wheelRotation = GET_WHEEL_DELTA_WPARAM(wParam);
			break;

		case WM_KEYDOWN:
		{
			if (wParam != 0x7A) // 0x7A = F11
				break;

			WindowMode newWindowMode = windowBinding[hwnd]->currentWindowMode == WINDOW_MODE_BORDERLESS_WINDOWED ? WINDOW_MODE_WINDOWED : WINDOW_MODE_BORDERLESS_WINDOWED; // take the other window mode then the one used right now
			windowBinding[hwnd]->ChangeWindowMode(newWindowMode);
			break;
		}

		case WM_INPUT: // when an input has been detected
		{
			UINT sizeOfStruct = sizeof(RAWINPUT);

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &sizeOfStruct, sizeof(RAWINPUTHEADER));
			LPBYTE bytePointer = new BYTE[sizeOfStruct];
			if (bytePointer == NULL)
				return 0;

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, bytePointer, &sizeOfStruct, sizeof(RAWINPUTHEADER)) != sizeOfStruct)
				throw std::runtime_error("GetRawInputData does not return correct size");

			RAWINPUT* rawInput = (RAWINPUT*)bytePointer;
			if (rawInput->header.dwType != RIM_TYPEMOUSE)
				break;

			windowBinding[hwnd]->cursorX = rawInput->data.mouse.lLastX;
			windowBinding[hwnd]->cursorY = rawInput->data.mouse.lLastY;

			delete[] bytePointer;
			break;
		}

		case WM_DROPFILES: // when a file has been dropped on the window
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
	windows.erase(this);
	UnregisterClassW(className.c_str(), hInstance);
}