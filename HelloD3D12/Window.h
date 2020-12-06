#pragma once
#include <Windows.h>
#include <optional>
#include "Events/Event.h"
#include "Input.h"
#include "Graphics.h"

struct WindowProps
{
	std::string Title;
	unsigned int Width;
	unsigned int Height;

	WindowProps(const std::string& title = "Windows Framework",
		unsigned int width = 1280,
		unsigned int height = 960)
		:
		Title(title),
		Width(width),
		Height(height)
	{}
};

class Window
{
public:
	using EventCallbackFn = std::function<void(Event&)>;

private:
	class WindowClass
	{
	public:
		static const LPCSTR GetName();
		static HINSTANCE GetInstance();

	private:
		WindowClass();
		~WindowClass();

		static constexpr const LPCSTR wndClassName = "Windows Framework";
		static WindowClass wndClass;
		HINSTANCE hInstance;
	};

public:
	Window(const WindowProps& props = WindowProps());
	~Window();

	void OnUpdate();

	inline unsigned int GetWidth() const { return m_Data.Width; }
	inline unsigned int GetHeight() const { return m_Data.Height; }

	inline void SetEventCallback(const EventCallbackFn& callback) { m_Data.EventCallback = callback; }
	void SetVSync(bool enabled);
	bool IsVSync() const;
	static std::optional<int> ProcessMessages();

	inline void* GetNativeWindow() const { return m_Window; }

	inline Graphics& GetGraphics() const { return *m_Graphics; }

private:
	struct WindowData
	{
		std::string Title;
		unsigned int Width, Height;
		bool VSync;

		EventCallbackFn EventCallback;
	};

	void Init(const WindowProps& props = WindowProps());
	void Shutdown();

	static LRESULT CALLBACK HandleMessageSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK HandleMessageThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	Input input;

private:
	void* m_Window;

	WindowData m_Data;

	HWND m_Hwnd;

	Graphics* m_Graphics;

};