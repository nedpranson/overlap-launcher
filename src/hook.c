#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <stdbool.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef struct {
    DWORD key;

    HHOOK hook;
    HANDLE proc;
    HANDLE wait;
} hk_proc;

static SRWLOCK lock = SRWLOCK_INIT;
static hk_proc* hooked_procs = NULL;

static HMODULE dll_mod = NULL;
static HOOKPROC dll_hook_proc = NULL;

static bool exiting = false;

// static HANDLE pipe;

static HWND get_owner_window(HWND wnd) {
    HWND owner = wnd;
    HWND tmp = NULL;

    while ((tmp = GetWindow(owner, GW_OWNER))) {
        owner = tmp;
    }

    return owner;
}

static bool is_win_10_background_window(HWND wnd) {
    BOOL is_cloaked = FALSE;
    DwmGetWindowAttribute(wnd, DWMWA_CLOAKED, &is_cloaked, sizeof(BOOL));
    return is_cloaked;
}

// https://github.com/lokeshgovindu/AltTab/blob/master/source/AltTabWindow.cpp#L1506
static bool is_alt_tab_window(HWND wnd) {
    if (IsWindowVisible(wnd)) {
        HWND owner = get_owner_window(wnd);

        if (GetLastActivePopup(owner) != wnd) {
            return false;
        }

        DWORD owner_es = GetWindowLong(owner, GWL_EXSTYLE);
        if (owner_es && IsWindowVisible(owner) && !((owner_es & WS_EX_TOOLWINDOW) && !(owner_es & WS_EX_APPWINDOW)) && !is_win_10_background_window(owner)) {
            return true;
        }

        DWORD wnd_es = GetWindowLong(wnd, GWL_EXSTYLE);
        if (wnd_es && !((wnd_es & WS_EX_TOOLWINDOW) && !(wnd_es & WS_EX_APPWINDOW)) && !is_win_10_background_window(wnd)) {
            return true;
        }

        if (wnd_es == 0 && owner_es == 0) {
            return true;
        }
    }

    return false;
}

static VOID CALLBACK
ProcessExitCallback(PVOID lpParameter, BOOLEAN /*TimerOrWaitFired*/) {
    DWORD pid = (DWORD)(uintptr_t)lpParameter;

    AcquireSRWLockExclusive(&lock);
    if (exiting) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }
    hk_proc hooked_proc = hmgets(hooked_procs, pid);
    hmdel(hooked_procs, pid);
    ReleaseSRWLockExclusive(&lock);

    UnhookWindowsHookEx(hooked_proc.hook);
    CloseHandle(hooked_proc.proc);
}

static VOID CALLBACK
WinEventProc(
    HWINEVENTHOOK /*hWinEventHook*/,
    DWORD /*event*/,
    HWND hwnd,
    LONG /*idObject*/,
    LONG /*idChild*/,
    DWORD /*idEventThread*/,
    DWORD /*dwmsEventTime*/) {

    if (!is_alt_tab_window(hwnd)) {
        return;
    }

    DWORD pid;
    DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    if (!tid) {
        return;
    }

    AcquireSRWLockExclusive(&lock);
    if (hmgeti(hooked_procs, pid) >= 0) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }

    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!proc) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }

    HHOOK hook = SetWindowsHookEx(WH_GETMESSAGE, dll_hook_proc, dll_mod, tid);
    if (!hook) {
        ReleaseSRWLockExclusive(&lock);
        CloseHandle(proc);
        return;
    }

    if (!PostThreadMessage(tid, WM_NULL, 0, 0)) {
        ReleaseSRWLockExclusive(&lock);
        UnhookWindowsHookEx(hook);
        CloseHandle(proc);
        return;
    }

    HANDLE wait;
    if (!RegisterWaitForSingleObject(
        &wait,
        proc,
        ProcessExitCallback,
        (PVOID)(uintptr_t)pid,
        INFINITE,
        WT_EXECUTEONLYONCE)) {

        ReleaseSRWLockExclusive(&lock);
        UnhookWindowsHookEx(hook);
        CloseHandle(proc);
        return;
    }

    // todo: notify that we hooked pid
    hk_proc hooked_proc = {
        .key = pid,
        .hook = hook,
        .proc = proc,
        .wait = wait,
    };

    hmputs(hooked_procs, hooked_proc);
    ReleaseSRWLockExclusive(&lock);
}

// would be nice to send to the app.c the pid we have hooked
// and allow app.c to send what pid to unhook
int main(int argc, char *argv[]) {
    // pipe = CreateFile(
    //     TEXT("\\\\.\\pipe\\OverlapPipe"),
    //     GENERIC_WRITE,
    //     0,
    //     NULL,
    //     OPEN_EXISTING,
    //     0,
    //     NULL);
    // if (pipe == INVALID_HANDLE_VALUE) {
    //     return 1;
    // }
    if (argc != 2) {
        return 1;
    }

    dll_mod = LoadLibraryA(argv[1]);
    if (!dll_mod) {
        return 1;
    }

    dll_hook_proc = (HOOKPROC)(void*)GetProcAddress(dll_mod, "__overlap_hook_proc");
    if (!dll_hook_proc) {
        FreeLibrary(dll_mod);
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
        // CloseHandle(pipe);
        FreeLibrary(dll_mod);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWinEvent(hook);

    AcquireSRWLockExclusive(&lock);
    exiting = true;
    ReleaseSRWLockExclusive(&lock);

    for (int i = 0; i < hmlen(hooked_procs); i++) {
        UnregisterWaitEx(hooked_procs[i].wait, INVALID_HANDLE_VALUE);

        UnhookWindowsHookEx(hooked_procs[i].hook);
        CloseHandle(hooked_procs[i].proc);
    }
    hmfree(hooked_procs);

    // CloseHandle(pipe);
    FreeLibrary(dll_mod);
    return 0;
}
