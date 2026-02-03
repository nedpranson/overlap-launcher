#include <windows.h>
#include <stdio.h>

static HANDLE pipe = NULL;

static VOID CALLBACK
WinEventProc(
    HWINEVENTHOOK /*hWinEventHook*/,
    DWORD /*event*/,
    HWND hwnd,
    LONG /*idObject*/,
    LONG /*idChild*/,
    DWORD /*idEventThread*/,
    DWORD /*dwmsEventTime*/) {

    char title[MAX_PATH];
    if (GetWindowTextA(hwnd, title, MAX_PATH)) {
        printf("%s\n", title);
    }
}

int main(int argc, char *argv[]) {
    pipe = CreateFile(
        TEXT("\\\\.\\pipe\\OverlapPipe"),
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        return 1;
    }

    HWINEVENTHOOK hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!hook) {
        CloseHandle(pipe);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWinEvent(hook);
    CloseHandle(pipe);
    return 0;
}
