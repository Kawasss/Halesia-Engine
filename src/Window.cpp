module;

#include <Windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <hidusage.h>

module System.Window;

import std;

#define BORDERLESS_WINDOWED WS_POPUP

std::map<HWND, Window*> Window::windowBinding;

MSG Window::message;

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
static std::string WinGetLastErrorAsString()
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

Window::Window(const Window::CreateInfo& createInfo)
{
	size = Point(createInfo.width, createInfo.height);

	coordinates = Point(createInfo.x, createInfo.y);
	mode = createInfo.windowMode;
	hIcon = reinterpret_cast<HICON>(LoadImageA(NULL, createInfo.icon.c_str(), IMAGE_ICON, 128, 128, LR_LOADFROMFILE));
	hCursor = LoadCursorW(NULL, IDC_ARROW);
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

	ExtendedStyle exStyle = createInfo.extendedStyle;
	Style style = Style::OverlappedWindow;

	if (createInfo.windowMode == Mode::BorderlessWindowed)
	{
		size.x = GetSystemMetrics(SM_CXSCREEN);
		size.y = GetSystemMetrics(SM_CYSCREEN);
		coordinates.x = coordinates.y = 0;
		exStyle = ExtendedStyle::AppWindow;
		style = Style::PopUpWindow;
	}

	hWindow = CreateWindowExA((DWORD)exStyle, className.c_str(), createInfo.name.c_str(), (DWORD)style, createInfo.x, createInfo.y, createInfo.width, createInfo.height, NULL, NULL, hInstance, NULL);

	windowBinding[hWindow] = this;

	if (hWindow == NULL)
		throw std::runtime_error("Failed to create a window: " + WinGetLastErrorAsString());

	maximized = createInfo.startMaximized;
	ShowWindow(hWindow, maximized);

	SetTimer(hWindow, 0, USER_TIMER_MINIMUM, NULL); //makes sure the window is being updated even when if the cursor isnt moving

#ifdef _DEBUG
	std::cout << "Created new window with dimensions " << size.x << 'x' << size.y << " and mode " << ModeToString(mode) << '\n';
#endif // _DEBUG
}

void Window::ApplyWindowMode()
{
	Monitor monitor = GetCurrentMonitor();
	
	coordinates.x = monitor.x;
	coordinates.y = monitor.y;

	if (mode == Mode::BorderlessWindowed)
	{
		size.x = monitor.width;
		size.y = monitor.height;

		SetWindowLongPtrA(hWindow, GWL_STYLE, BORDERLESS_WINDOWED);
		if (SetWindowPos(hWindow, NULL, coordinates.x, coordinates.y, size.x, size.y, SWP_SHOWWINDOW) == 0)
			throw std::runtime_error("Failed to resize the window to borderless mode: " + WinGetLastErrorAsString());
	}
	else if (mode == Mode::Windowed)
	{
		size.x = monitor.width / 2;
		size.y = monitor.height / 2;

		SetWindowLongPtrA(hWindow, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		if (SetWindowPos(hWindow, NULL, coordinates.x + size.x / 2, coordinates.y + size.y / 2, size.x, size.y, SWP_SHOWWINDOW) == 0)
			throw std::runtime_error("Failed to resize the window to windowed mode: " + WinGetLastErrorAsString());
	}
	events |= EVENT_VISIBILITY_CHANGE;
}

void Window::SetWindowMode(Window::Mode windowMode)
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
		window->wheelRotation = 0;
		window->prevCursor = window->absCursor;

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

int Window::GetMonitorWidth()
{
	return GetSystemMetrics(SM_CXSCREEN);
}

int Window::GetMonitorHeight()
{
	return GetSystemMetrics(SM_CYSCREEN);
}

void Window::GetRelativeCursorPosition(int& x, int& y) const
{
	x = absCursor.x - prevCursor.x;
	y = absCursor.y - prevCursor.y;
}

std::string_view Window::ModeToString(Window::Mode mode)
{
	switch (mode)
	{
	case Window::Mode::BorderlessWindowed:
		return "BorderlessWindowed";
	case Window::Mode::Windowed:
		return "Windowed";
	}
	return "Unknown";
}

Monitor Window::GetCurrentMonitor() const
{
	HMONITOR monitor = ::MonitorFromWindow(hWindow, MONITOR_DEFAULTTOPRIMARY);

	MONITORINFO info{};
	info.cbSize = sizeof(info);
	BOOL _ = GetMonitorInfoA(monitor, &info); // should not fail so dont check it

	Monitor ret{};
	ret.x = info.rcMonitor.left;
	ret.y = info.rcMonitor.top;

	ret.width  = info.rcMonitor.right - info.rcMonitor.left;
	ret.height = info.rcMonitor.bottom - info.rcMonitor.top;

	return ret;
}

void Window::HandleEvents()
{
	if (events & EVENT_MODE_CHANGE)
		ApplyWindowMode();
	if (events & EVENT_RESIZE)
		SetWindowPos(hWindow, NULL, coordinates.x, coordinates.y, size.x, size.y, SWP_SHOWWINDOW);
	if (events & EVENT_CURSOR_CHANGE)
		ShowCursor(lockCursor);
	if (events & EVENT_VISIBILITY_CHANGE)
		ShowWindow(hWindow, maximized ? SW_RESTORE : 0);

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

		if (wParam == SIZE_MINIMIZED)
		{
			window->size.x = 0;
			window->size.y = 0;
		}
		else
		{
			window->size.x = LOWORD(lParam);
			window->size.y = HIWORD(lParam);
		}
		break;

	case WM_MOVE:
		if (window->mode == Mode::Windowed)
		{
			window->coordinates.x = LOWORD(lParam);
			window->coordinates.y = HIWORD(lParam);
		}
		else
		{
			Monitor monitor = window->GetCurrentMonitor();

			window->coordinates.x = monitor.x;
			window->coordinates.y = monitor.y;

			window->SetWidthAndHeight(monitor.width, monitor.height);
		}
		break;

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
			window->SetWindowMode(window->mode == Mode::BorderlessWindowed ? Mode::Windowed : Mode::BorderlessWindowed);
		break;

	case WM_DROPFILES: // when a file has been dropped on the window
	{
		HDROP hDrop = (HDROP)wParam;
		UINT bufferSize = DragQueryFileA(hDrop, 0, NULL, 512); // DragQueryFileA returns the length of the path of the file if the pointer to the file buffer is NULL

		std::vector<char> fileBuffer(bufferSize + 1);
		DragQueryFileA(hDrop, 0, fileBuffer.data(), bufferSize + 1); // could also directly write to the string ??

		window->containsDroppedFile = true;
		window->droppedFile = std::string(fileBuffer.data());

#ifdef _DEBUG
		std::cout << "Detected dropped file: " + window->droppedFile << std::endl;
#endif
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
	DestroyWindow(hWindow);
	windowBinding.erase(hWindow);
	UnregisterClassA(className.c_str(), hInstance);
}