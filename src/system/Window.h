#pragma once
#include <Windows.h>
#include <string>
#include <map>
#include <set>

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
	AppWindow = WS_EX_APPWINDOW,
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
	std::string windowName, className = "GenericWindow";

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

class Window
{
	public:
		HWND window;
		HINSTANCE hInstance;
		LRESULT(*additionalPollCallback)(HWND, UINT, WPARAM, LPARAM) = nullptr;

		Window(const Win32WindowCreateInfo& createInfo);
		~Window();

		static void PollMessages();

		bool resized = false;
		
		bool cursorHasMoved = false;

		std::string GetDroppedFile();
		WindowMode GetWindowMode() const { return mode; }

		bool ContainsDroppedFile() const { return containsDroppedFile; }
		bool ShouldClose()         const { return shouldClose;         }
		bool CursorIsLocked()      const { return lockCursor;          }

		int GetX()             const { return coordinates.x; }
		int GetY()             const { return coordinates.y; }
		int GetWidth()         const { return size.x;        }
		int GetHeight()        const { return size.y;        }
		int GetWheelRotation() const { return wheelRotation; }
		bool IsMaximized()     const { return maximized;     }

		void SetWidth(int value);
		void SetHeight(int value);
		void SetWidthAndHeight(int width, int height); // more efficient since it only issues one windows api call
		void SetXAndY(int x, int y);
		void SetMaximized(bool val);

		void GetRelativeCursorPosition(int& x, int& y) const { x = cursor.x; y = cursor.y;       }
		void GetAbsoluteCursorPosition(int& x, int& y) const { x = absCursor.x; y = absCursor.y; }
		void LockCursor();
		void UnlockCursor();
		void SetWindowMode(WindowMode windowMode);

		void Destroy();

	private:
		struct Point
		{
			Point() = default;
			Point(int v1, int v2) : x(v1), y(v2) {}

			int x = 0, y = 0;

			void Clear() { x = y = 0; }
			bool operator==(const Point& other) { return x == other.x && y == other.y; }
		};

		enum Event : uint8_t
		{
			EVENT_RESIZE = 1 << 0,
			EVENT_CURSOR_CHANGE = 1 << 1,
			EVENT_VISIBILITY_CHANGE = 1 << 2,
			EVENT_MODE_CHANGE = 1 << 3,
		};
		uint8_t events;

		void HandleEvents();
		void ApplyWindowMode();

		static std::map<HWND, Window*> windowBinding;

		static MSG message;

		HICON   hIcon;
		HCURSOR hCursor;

		std::string droppedFile, className;

		Point size;
		Point coordinates;
		Point cursor;
		Point absCursor;
		WindowMode mode;

		bool lockCursor = false;
		bool maximized  = true; // if this is false the window should be minimized

		int wheelRotation = 0;
		bool shouldClose = false, containsDroppedFile = false;

		static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};