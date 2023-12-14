#include "core/engine.h"
#include "imgui/imgui_impl_win32.h"
#include "rpmalloc/rpmalloc.h"
#include "resource.h"
#include <Windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{   
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            Engine::GetInstance()->WindowResizeSignal(hWnd, LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

static eastl::string GetWorkPath()
{
    wchar_t exe_file[MAX_PATH];
    GetModuleFileName(NULL, exe_file, MAX_PATH);

    DWORD size = WideCharToMultiByte(CP_ACP, 0, exe_file, -1, NULL, 0, NULL, FALSE);

    eastl::string work_path;
    work_path.resize(size);

    WideCharToMultiByte(CP_ACP, 0, exe_file, -1, (LPSTR)work_path.c_str(), size, NULL, FALSE);

    size_t last_slash = work_path.find_last_of('\\');
    return work_path.substr(0, last_slash + 1);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    rpmalloc_initialize();
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    windowClass.lpszClassName = L"REWindowClass";
    RegisterClassEx(&windowClass);

    RECT desktopRect;
    GetClientRect(GetDesktopWindow(), &desktopRect);

    const unsigned int desktop_width = desktopRect.right - desktopRect.left;
    const unsigned int desktop_height = desktopRect.bottom - desktopRect.top;

    const unsigned int window_width = min(desktop_width * 0.9, 1920);
    const unsigned int window_height = min(desktop_height * 0.9, 1080);

    RECT windowRect = { 0, 0, (LONG)window_width, (LONG)window_height };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int x = (desktop_width - (windowRect.right - windowRect.left)) / 2;
    int y = (desktop_height - (windowRect.bottom - windowRect.top)) / 2;

    HWND hwnd = CreateWindow(
        windowClass.lpszClassName,
        L"Real Engine",
        WS_OVERLAPPEDWINDOW,
        x,
        y,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        nullptr);

    ShowWindow(hwnd, nCmdShow);

    Engine::GetInstance()->Init(GetWorkPath(), hwnd, window_width, window_height);

    // Main loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Engine::GetInstance()->Tick();
    }

    Engine::GetInstance()->Shut();

    return 0;
}
