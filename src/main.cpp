#include <windows.h>

namespace {
constexpr wchar_t window_class[] = L"MedAurasWindow";

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(window, message, w_param, l_param);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    const WNDCLASS window_definition{
        .lpfnWndProc = window_procedure,
        .hInstance = instance,
        .lpszClassName = window_class,
    };

    if (!RegisterClass(&window_definition)) {
        return 1;
    }

    const HWND window = CreateWindowEx(
        0,
        window_class,
        L"Medication Cooldown Widget",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        420,
        180,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window) {
        return 1;
    }

    ShowWindow(window, show_command);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return static_cast<int>(message.wParam);
}
