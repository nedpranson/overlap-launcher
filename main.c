#include <windows.h>

#define WM_TRAYICON (WM_USER + 1)

#define TRAY_TITLE 2
#define TRAY_EXIT 2

__declspec(dllexport) void __overlap_ignore_proc(void) {}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                POINT point;

                GetCursorPos(&point);
                SetForegroundWindow(hWnd);

                HMENU hMenu = (HMENU)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

                TrackPopupMenu(
                    hMenu,
                    TPM_RIGHTBUTTON,
                    point.x,
                    point.y,
                    0,
                    hWnd,
                    NULL);
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == TRAY_EXIT) {
                PostQuitMessage(0);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int main(int argc, char* argv[]) {
    int bId = MessageBoxW(
        NULL,
        L"Overlap is currently at ALPHA release.\n\n"
        L"You should expect crashes, instability, and unexpected behavior.\n\n"
        L"If you encounter any issues, feel free to report them at:\n"
        L"https://github.com/nedpranson/overlap/issues\n\n"
        L"Do you want to continue?",
        L"Overlap – Alpha Warning",
        MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2);

    if (bId != IDOK) {
        return 0;
    }

    MessageBoxW(
        NULL,
        L"Overlap will not be updated automatically for now.\n\n"
        L"Any future updates must be downloaded manually from the Overlap GitHub repository:\n"
        L"https://github.com/nedpranson/overlap\n\n"
        L"Click OK to proceed.",
        L"Overlap – Update Notice",
        MB_ICONINFORMATION | MB_OK);

    if (argc != 2) {
        return 1;
    }

    HMODULE libModule = LoadLibraryA(argv[1]);
    if (!libModule) {
        return 1;
    }

    FARPROC proc = GetProcAddress(libModule, "__overlap_hook_proc");
    if (!proc) {
        FreeLibrary(libModule);
        return 1;
    }

    HINSTANCE hInstance = GetModuleHandleW(NULL);
    if (!hInstance) {
        FreeLibrary(libModule);
        return 1;
    }

    WNDCLASSW wndClass = {0};
    wndClass.lpfnWndProc = WndProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = L"OverlapTrayClass";

    if (!RegisterClassW(&wndClass)) {
        FreeLibrary(libModule);
        return 1;
    };

    HWND hWnd = CreateWindowW(
        wndClass.lpszClassName,
        L"Overlap Tray Window",
        WS_OVERLAPPED,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!hWnd) {
        FreeLibrary(libModule);
        return 1;
    }

    HICON icon = LoadImageW(
        NULL,
        (LPWSTR)IDI_APPLICATION,
        IMAGE_ICON,
        16,
        16,
        LR_SHARED);

    if (!icon) {
        FreeLibrary(libModule);
        return 1;
    }

    HMENU hMenu = CreatePopupMenu();
    if (hMenu == NULL) {
        FreeLibrary(libModule);
        return 1;
    }

    AppendMenuW(hMenu, MF_STRING, TRAY_TITLE, L"Overlap");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, TRAY_EXIT, L"Exit");

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)hMenu);

    NOTIFYICONDATAW data = {0};
    data.cbSize = sizeof(data);
    data.hWnd = hWnd;
    data.uID = 1;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    data.hIcon = icon;
    data.uCallbackMessage = WM_TRAYICON;
    lstrcpyW(data.szTip, L"Overlap");

    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        DestroyMenu(hMenu);
        FreeLibrary(libModule);
        return 1;
    }

    HHOOK hook = SetWindowsHookEx(WH_CBT, (HOOKPROC)proc, libModule, 0);
    if (!hook) {
        Shell_NotifyIconW(NIM_DELETE, &data);
        DestroyMenu(hMenu);
        FreeLibrary(libModule);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(hook);
    Shell_NotifyIconW(NIM_DELETE, &data);
    DestroyMenu(hMenu);
    FreeLibrary(libModule);

    return 0;
}
