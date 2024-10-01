#include "system/Window.h"
#include <shellapi.h>
#include <windowsx.h>
#include <iostream>
#include <stdexcept>

#define BORDERLESS_WINDOWED WS_POPUP

std::map<HWND, Window*> Window::windowBinding;

MSG Window::message;

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

Window::Window(const Win32WindowCreateInfo& createInfo)
{
	size = Point(createInfo.width, createInfo.height);

	coordinates = Point(createInfo.x, createInfo.y);
	mode = createInfo.windowMode;
	hIcon = createInfo.icon;
	hCursor = createInfo.cursor;
	className = createInfo.className;

	hInstance = GetModuleHandle(NULL);

	WNDCLASSEXA wc{};
	wc.cbSize = sizeof(WNDCLASSEXA);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = hIcon;
	wc.hIconSm = hIcon;
	wc.hCursor = hCursor;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = createInfo.className.c_str();

	if (!RegisterClassExA(&wc))
		throw std::runtime_error("Failed to register a window: " + WinGetLastErrorAsString());

	ExtendedWindowStyle exStyle = createInfo.extendedWindowStyle;
	WindowStyle style = WindowStyle::OverlappedWindow;

	if (createInfo.windowMode == WINDOW_MODE_BORDERLESS_WINDOWED)
	{
		size.x = GetSystemMetrics(SM_CXSCREEN);
		size.y = GetSystemMetrics(SM_CYSCREEN);
		coordinates.x = coordinates.y = 0;
		exStyle = ExtendedWindowStyle::AppWindow;
		style = WindowStyle::PopUpWindow;
	}

	window = CreateWindowExA((DWORD)exStyle, className.c_str(), createInfo.windowName.c_str(), (DWORD)style, createInfo.x, createInfo.y, createInfo.width, createInfo.height, NULL, NULL, hInstance, NULL);

	windowBinding[window] = this;

	if (window == NULL)
		throw std::runtime_error("Failed to create a window: " + WinGetLastErrorAsString());

	maximized = createInfo.startMaximized;
	ShowWindow(window, maximized);

	SetTimer(window, 0, USER_TIMER_MINIMUM, NULL); //makes sure the window is being updated even when if the cursor isnt moving

	RAWINPUTDEVICE rawInputDevice{};
	rawInputDevice.usUsagePage = 0x01;
	rawInputDevice.usUsage = 0x02;
	rawInputDevice.dwFlags = 0; // no flags
	rawInputDevice.hwndTarget = NULL;

	if (!RegisterRawInputDevices(&rawInputDevice, 1, sizeof(rawInputDevice)))
		throw std::runtime_error("Failed to register the raw input device for the mouse");

#ifdef _DEBUG
	std::string message = "Created new window with dimensions " + std::to_string(size.x) + 'x' + std::to_string(size.y) + " and mode " + WindowModeToString(mode);
	std::cout << message << std::endl;
#endif
}

void Window::ApplyWindowMode()
{
	HMONITOR monitor = MonitorFromWindow(window, 0);

	MONITORINFOEXA monitorInfo{};
	monitorInfo.cbSize = sizeof(monitorInfo);
	if (!GetMonitorInfoA(monitor, &monitorInfo))
		throw std::runtime_error("Failed to fetch relevant monitor info");

	int maxWidth  = abs(monitorInfo.rcMonitor.left - monitorInfo.rcMonitor.right);
	int maxHeight = abs(monitorInfo.rcMonitor.top - monitorInfo.rcMonitor.bottom);

	coordinates.x = monitorInfo.rcMonitor.left;
	coordinates.y = monitorInfo.rcMonitor.top;

	if (mode == WINDOW_MODE_BORDERLESS_WINDOWED)
	{
		size.x = maxWidth;
		size.y = maxHeight;

		SetWindowLongPtrA(window, GWL_STYLE, BORDERLESS_WINDOWED);
		if (SetWindowPos(window, NULL, 0, 0, size.x, size.y, SWP_FRAMECHANGED) == 0)
			throw std::runtime_error("Failed to resize the window to borderless mode: " + WinGetLastErrorAsString());
	}
	else if (mode == WINDOW_MODE_WINDOWED)
	{
		size.x = maxWidth / 2;
		size.y = maxHeight / 2;

		SetWindowLongPtrA(window, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		if (SetWindowPos(window, NULL, coordinates.x + size.x / 2, coordinates.y + size.y / 2, size.x, size.y, SWP_FRAMECHANGED) == 0)
			throw std::runtime_error("Failed to resize the window to windowed mode: " + WinGetLastErrorAsString());
	}
	events |= EVENT_VISIBILITY_CHANGE;
}

void Window::SetWindowMode(WindowMode windowMode)
{
	if (windowMode == mode)
		return;
	mode = windowMode;
	events |= EVENT_MODE_CHANGE;	
}

void Window::PollMessages()
{
	for (const auto& [handle, window] : windowBinding)
	{
		window->cursor.Clear();
		window->wheelRotation = 0;

		window->HandleEvents();
	}
	while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}
}

void Window::SetXAndY(int x, int y)
{
	if (coordinates.x == x && coordinates.y == y)
		return;
	coordinates.x = x;
	coordinates.y = y;
	events |= EVENT_RESIZE;
}

void Window::SetWidth(int value)
{
	SetWidthAndHeight(value, size.y);
}

void Window::SetHeight(int value)
{
	SetWidthAndHeight(size.x, value);
}

void Window::SetWidthAndHeight(int width, int height)
{
	if (size.x == width && size.y == height)
		return;
	size.x = width;
	size.y = height;

	events |= EVENT_RESIZE;
}

void Window::SetMaximized(bool val)
{
	maximized = val;
	events |= EVENT_VISIBILITY_CHANGE;
}

void Window::LockCursor()
{
	lockCursor = true;
	events |= EVENT_CURSOR_CHANGE;
}

void Window::UnlockCursor()
{
	lockCursor = false;
	events |= EVENT_CURSOR_CHANGE;
}

std::string Window::GetDroppedFile()
{
	containsDroppedFile = false;
	return droppedFile;
}

void Window::HandleEvents()
{
	if (events & EVENT_MODE_CHANGE)
		ApplyWindowMode();
	if (events & EVENT_RESIZE)
		SetWindowPos(window, NULL, coordinates.x, coordinates.y, size.x, size.y, SWP_FRAMECHANGED);
	if (events & EVENT_CURSOR_CHANGE)
		ShowCursor(lockCursor);
	if (events & EVENT_VISIBILITY_CHANGE)
		ShowWindow(window, maximized ? SW_RESTORE : 0);

	events = 0;
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (windowBinding.count(hwnd) == 0)
		return DefWindowProcA(hwnd, message, wParam, lParam);

	Window* window = windowBinding[hwnd];

	if (window->additionalPollCallback != nullptr)
		window->additionalPollCallback(hwnd, message, wParam, lParam);

	switch (message)
	{
	case WM_CREATE: // when the window is created
		break;
	case WM_CLOSE: // when the window should close
		window->shouldClose = true;
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY: // when the window is getting destroyed
		PostQuitMessage(0);
		break;

	case WM_SIZING:
	case WM_SIZE: // when the window is resized
		window->resized = true;

		window->size.x = LOWORD(lParam);
		window->size.y = HIWORD(lParam);
		break;

	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
	{
		WINDOWPOS* ptr = (WINDOWPOS*)lParam;
		window->coordinates.x = ptr->x;
		window->coordinates.y = ptr->y;
		break;
	}

	case WM_MOUSEMOVE: // when the cursor has moved
		if (window->lockCursor) // if the cursor is locked it has to stay inside the window, so this locks resets it to the center of the screen
			SetCursorPos(window->size.x / 2, window->size.y / 2);

		window->absCursor.x = GET_X_LPARAM(lParam);
		window->absCursor.y = GET_Y_LPARAM(lParam);
		break;

	case WM_MOUSEWHEEL: // when the mouse wheel has moved
		window->wheelRotation = GET_WHEEL_DELTA_WPARAM(wParam);
		break;

	case WM_KEYDOWN:
		if (wParam == VK_F11)
			window->SetWindowMode(window->mode == WINDOW_MODE_BORDERLESS_WINDOWED ? WINDOW_MODE_WINDOWED : WINDOW_MODE_BORDERLESS_WINDOWED);
		break;

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

		window->cursor.x = rawInput->data.mouse.lLastX;
		window->cursor.y = rawInput->data.mouse.lLastY;

		delete[] bytePointer;
		break;
	}

	case WM_DROPFILES: // when a file has been dropped on the window
	{
		HDROP hDrop = (HDROP)wParam;
		UINT bufferSize = DragQueryFileA(hDrop, 0, NULL, 512); // DragQueryFileA returns the length of the path of the file if the pointer to the file buffer is NULL
		char* fileBuffer = new char[bufferSize + 1]; // + 1 to account for the null terminator
		DragQueryFileA(hDrop, 0, fileBuffer, bufferSize + 1);

		window->containsDroppedFile = true;
		window->droppedFile = std::string(fileBuffer);

#ifdef _DEBUG
		std::cout << "Detected dropped file: " + window->droppedFile << std::endl;
#endif

		delete[] fileBuffer;
		break;
	}

	case WM_TIMER:
		break;

	default:
		return DefWindowProcA(hwnd, message, wParam, lParam);
	}
	return 0;
}

void Window::Destroy()
{
	delete this;
}

Window::~Window()
{
	DestroyWindow(window);
	windowBinding.erase(window);
	UnregisterClassA(className.c_str(), hInstance);
}