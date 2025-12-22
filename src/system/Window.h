#pragma once
#include <Windows.h>
#include <string>
#include <map>

struct Monitor
{
	int x, y;
	uint32_t width, height;
};

class Window
{
public:
	enum class Mode
	{
		BorderlessWindowed,
		Windowed,
	};

	enum class Style : uint32_t // redundant
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

	enum class ExtendedStyle : uint32_t // uint32_t as underlying type to combat int's incompetence
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

	struct CreateInfo
	{
		static constexpr int DIMENSION_DEFAULT = static_cast<int>(0x80000000); // from WinUser.h

		std::string name;
		std::string className = "GenericWindowClass";

		int width  = DIMENSION_DEFAULT;
		int height = DIMENSION_DEFAULT;

		int x = 0;
		int y = 0;

		std::string icon;

		Window::Mode windowMode = Mode::Windowed;

		Window::Style style = Style::OverlappedWindow;
		Window::ExtendedStyle extendedStyle = ExtendedStyle::SunkenEdgeBorder;

		bool startMaximized = false;
	};

	LRESULT(*additionalPollCallback)(HWND, UINT, WPARAM, LPARAM) = nullptr;

	Window(const Window::CreateInfo& createInfo);
	~Window();

	static void PollMessages();
		
	static int GetMonitorWidth();
	static int GetMonitorHeight();

	static std::string_view ModeToString(Window::Mode mode);

	bool resized = false;
		
	bool cursorHasMoved = false;

	std::string GetDroppedFile();
	Window::Mode GetWindowMode() const { return mode; }

	bool ContainsDroppedFile() const { return containsDroppedFile; }
	bool ShouldClose()         const { return shouldClose;         }
	bool CursorIsLocked()      const { return lockCursor;          }

	int GetX()             const { return coordinates.x; }
	int GetY()             const { return coordinates.y; }
	int GetWidth()         const { return size.x;        }
	int GetHeight()        const { return size.y;        }
	int GetWheelRotation() const { return wheelRotation; }

	bool IsMaximized()     const { return maximized;     }
	bool CanBeRenderedTo() const { return size.x != 0 && size.y != 0; }

	HWND GetHandle()        const { return hWindow; }
	HINSTANCE GetInstance() const { return hInstance; }

	Monitor GetCurrentMonitor() const;

	void SetWidth(int value);
	void SetHeight(int value);
	void SetWidthAndHeight(int width, int height); // more efficient since it only issues one windows api call
	void SetXAndY(int x, int y);
	void SetMaximized(bool val);

	void GetRelativeCursorPosition(int& x, int& y) const;
	void GetAbsoluteCursorPosition(int& x, int& y) const { x = absCursor.x; y = absCursor.y; }
	void LockCursor();
	void UnlockCursor();
	void SetWindowMode(Window::Mode windowMode);

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
		EVENT_NONE              = 0 << 0,
		EVENT_RESIZE            = 1 << 0,
		EVENT_CURSOR_CHANGE     = 1 << 1,
		EVENT_VISIBILITY_CHANGE = 1 << 2,
		EVENT_MODE_CHANGE       = 1 << 3,
	};
	uint8_t events = EVENT_NONE;

	void HandleEvents();
	void ApplyWindowMode();

	static std::map<HWND, Window*> windowBinding;

	static MSG message;

	HWND hWindow;
	HINSTANCE hInstance;

	HICON   hIcon;
	HCURSOR hCursor;

	std::string droppedFile, className;

	Point size;
	Point coordinates;
	Point absCursor;
	Point prevCursor;
	Window::Mode mode;

	bool lockCursor = false;
	bool maximized  = true; // if this is false the window should be minimized

	int wheelRotation = 0;
	bool shouldClose = false, containsDroppedFile = false;

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};