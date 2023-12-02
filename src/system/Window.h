#pragma once
#include <Windows.h>
#include <string>
#include <map>
#include <unordered_set>

enum WindowMode // enum for scalability
{
	WINDOW_MODE_WINDOWED,
	WINDOW_MODE_BORDERLESS_WINDOWED
};

inline std::string WindowModeToString(WindowMode windowMode)
{
	switch (windowMode)
	{
	case WINDOW_MODE_BORDERLESS_WINDOWED:
		return "WINDOW_MODE_BORDERLESS_WINDOWED";
	case WINDOW_MODE_WINDOWED:
		return "WINDOW_MODE_WINDOWED";
	default:
		return "";
	}
}

enum class ExtendedWindowStyle : uint32_t // uint32_t as underlying type to combat int's incompetence
{
	DragAndDropFiles = WS_EX_ACCEPTFILES,
	RaisedEdgeBorder = WS_EX_WINDOWEDGE,
	SunkenEdgeBorder = WS_EX_CLIENTEDGE,
	OverlappedWindow = WS_EX_OVERLAPPEDWINDOW,
	Transparent = WS_EX_TRANSPARENT,
	RightScrollBar = WS_EX_RIGHTSCROLLBAR,
	LeftScrollBar = WS_EX_LEFTSCROLLBAR,
	RightAlignedProperties = WS_EX_RIGHT,
	LeftAlignedProperties = WS_EX_LEFT,
	PalleteWindow = WS_EX_PALETTEWINDOW,
	StaticEdge = WS_EX_STATICEDGE,
};

enum class WindowStyle : uint32_t // redundant
{
	Minimized = WS_MINIMIZE,
	Maximized = WS_MAXIMIZE,
	MinimizeBox = WS_MINIMIZEBOX,
	MaximizeBox = WS_MAXIMIZEBOX,
	Disabled = WS_DISABLED,
	SizingBorder = WS_THICKFRAME,
	ThinLineBorder = WS_BORDER,
	HorizontalScrollBar = WS_HSCROLL,
	VerticalScrollBar = WS_VSCROLL,
	OverlappedWindow = WS_OVERLAPPEDWINDOW,
	PopUpWindow = WS_POPUPWINDOW,
	PopUp = WS_POPUP,
	WindowMenu = WS_SYSMENU,
	TitleBar = WS_CAPTION
};

struct Win32WindowCreateInfo
{
	std::wstring windowName = L"", className = L"GenericWindow";

	int width = CW_USEDEFAULT, height = CW_USEDEFAULT, x = CW_USEDEFAULT, y = CW_USEDEFAULT;

	HICON icon = LoadIconW(NULL, IDI_APPLICATION);
	HCURSOR cursor = LoadCursorW(NULL, IDC_ARROW);

	WindowMode windowMode = WINDOW_MODE_WINDOWED;
	WindowStyle style = WindowStyle::OverlappedWindow;
	ExtendedWindowStyle extendedWindowStyle = ExtendedWindowStyle::SunkenEdgeBorder;

	bool startMaximized = false;
	bool setTimer = true;
	unsigned int timeOutValue = USER_TIMER_MINIMUM;
};

const int monitorWidth = GetSystemMetrics(SM_CXSCREEN);
const int monitorHeight = GetSystemMetrics(SM_CYSCREEN);

class Win32Window
{
	public:
		HWND window;
		HINSTANCE hInstance;
		LRESULT(*additionalPollCallback)(HWND, UINT, WPARAM, LPARAM) = nullptr;

		Win32Window(const Win32WindowCreateInfo& createInfo);
		~Win32Window();

		static void PollMessages();

		bool resized = false;
		bool maximized = true; // if this is false the window should be minimized
		bool cursorHasMoved = false;

		std::string GetDroppedFile();
		WindowMode GetWindowMode();

		bool ContainsDroppedFile();
		bool ShouldClose();
		bool CursorIsLocked();

		int GetWidth();
		int GetHeight();
		int GetWheelRotation();

		void GetRelativeCursorPosition(int& x, int& y);
		void GetAbsoluteCursorPosition(int& x, int& y);
		void LockCursor();
		void UnlockCursor();
		void ChangeWindowMode(WindowMode windowMode);

		void Destroy();

	private:
		static MSG message;
		HICON icon;
		HCURSOR cursor;
		static std::map<HWND, Win32Window*> windowBinding;
		static std::unordered_set<Win32Window*> windows;
		std::string droppedFile = "";
		std::wstring className = L"", windowName = L"";
		int width = 0, height = 0, x = 0, y = 0, cursorX = 0, cursorY = 0, absCursorX = 0, absCursorY = 0, wheelRotation = 0;
		bool shouldClose = false, lockCursor = false, containsDroppedFile = false;
		WindowMode currentWindowMode;
		ExtendedWindowStyle extendedWindowStyle;
		
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};