#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <stdbool.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_D3D9_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_d3d9.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 400

#define WM_TRAYICON (WM_USER + 1)

#define TRAY_TITLE 1
#define TRAY_EXIT 2

__declspec(dllexport) void __overlap_ignore_proc(void) {}

#define CONTINUE_IF(ok) if (!ok) { \
    result = 1; \
    goto cleanup; \
}

#define COL_BG        nk_rgba(16, 25, 30, 255)   // #10191E
#define COL_BG_SOFT   nk_rgba(22, 32, 38, 255)
#define COL_BG_HOVER  nk_rgba(28, 38, 44, 255)

#define COL_TEXT      nk_rgba(255, 255, 255, 255)
#define COL_TEXT_MUTED nk_rgba(170, 180, 185, 255)

#define COL_BORDER    nk_rgba(40, 55, 60, 255)

#define COL_ACCENT    nk_rgba(0, 223, 162, 255) // #00DFA2
#define COL_ACCENT_SOFT nk_rgba(0, 223, 162, 120)

#define COL_INV_BG    nk_rgba(255, 255, 255, 255)
#define COL_INV_TEXT  nk_rgba(0, 0, 0, 255)

// | -----------------|
// | Logo            :|
// |------------------|
// | O Hollow Knight  |
// | O Some other...  |
// | ...              |
// | ..               |
// | .                |
// |------------------|

static LRESULT CALLBACK
WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    if (nk_d3d9_handle_event(hWnd, uMsg, wParam, lParam)) {
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static HWND GetOwnerWindow(HWND hWnd) {
    HWND hOwner = hWnd;
    HWND hTmp = NULL;

    while ((hTmp = GetWindow(hOwner, GW_OWNER))) {
        hOwner = hTmp;
    }

    return hOwner;
}

static bool 
IsAppWindow(HWND hWnd) {
    DWORD exStyles = GetWindowLong(hWnd, GWL_EXSTYLE);
    return exStyles && !((exStyles & WS_EX_TOOLWINDOW) && !(exStyles & WS_EX_APPWINDOW));
}

static bool IsWin10BackgroundWindow(HWND hWnd) {
  BOOL flag = FALSE;
  DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &flag, sizeof(BOOL));
  return flag;
}

static bool IsTaskbarWindow(HWND hWnd) {
    if (IsWindowVisible(hWnd)) {
        HWND hOwner = GetOwnerWindow(hWnd);

        if (GetLastActivePopup(hOwner) != hWnd) {
            return false;
        }

        if (IsWindowVisible(hOwner) && IsAppWindow(hOwner) && !IsWin10BackgroundWindow(hOwner)) {
            return true;
        }

        if (IsAppWindow(hWnd) && !IsWin10BackgroundWindow(hWnd)) {
            return true;
        }
    }
    return false;
}

static char* AllocWindowTextA(HWND hWnd) {
    int len = GetWindowTextLengthA(hWnd);
    if (len == 0) {
        return NULL;
    }

    char* text = (char*)malloc(len + 1);
    if (text) {
        GetWindowTextA(hWnd, text, len + 1);
    }
    return text;
}

//static VOID CALLBACK
//WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
    //(void)hWinEventHook;
    //(void)event;
    //(void)idObject;
    //(void)idChild;
    //(void)idEventThread;
    //(void)dwmsEventTime;

    //assert(event == EVENT_OBJECT_FOCUS);
    //if (!IsTaskbarWindow(hWnd)) return;

    //DWORD processId;
    //if (GetWindowThreadProcessId(hWnd, &processId) == 0) {
        //return;
    //}
//}

static BOOL CALLBACK
EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    (void)lParam;

    char* hWndTitle;
    if (IsTaskbarWindow(hWnd) && (hWndTitle = AllocWindowTextA(hWnd))) {
        printf("%s\n", hWndTitle);
        free(hWndTitle);
    }

    return TRUE;
}

static HRESULT CreateD3D9Device(HWND hWnd, IDirect3DDevice9Ex** device) {
    HRESULT hr;

    D3DPRESENT_PARAMETERS params = { 0 };
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    params.BackBufferWidth = WINDOW_WIDTH;
    params.BackBufferHeight = WINDOW_HEIGHT;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 1;
    params.MultiSampleType = D3DMULTISAMPLE_NONE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = hWnd;
    params.EnableAutoDepthStencil = TRUE;
    params.AutoDepthStencilFormat = D3DFMT_D24S8;
    params.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    params.Windowed = TRUE;

    IDirect3D9Ex* d3d9;
    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IDirect3D9Ex_CreateDeviceEx(
        d3d9,
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE,
        &params,
        NULL,
        device);

    IDirect3D9Ex_Release(d3d9);
    return hr;
}

static HRESULT RenderD3D9Objects(IDirect3DDevice9Ex* device) {
    HRESULT hr;

    hr = IDirect3DDevice9Ex_Clear(
        device,
        0,
        NULL,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
        D3DCOLOR_COLORVALUE(1.00f, 1.0f, 1.0f, 1.0f), 0.0f, 0);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IDirect3DDevice9Ex_BeginScene(device);
    if (FAILED(hr)) {
        return hr;
    }

    nk_d3d9_render(NK_ANTI_ALIASING_ON);

    hr = IDirect3DDevice9Ex_EndScene(device);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IDirect3DDevice9Ex_PresentEx(device, NULL, NULL, NULL, NULL, 0);
    return hr;
}

static void SetDarkTheme(struct nk_context* ctx) {
    struct nk_color table[NK_COLOR_COUNT];

    // todo: pick actual nice colors

    table[NK_COLOR_TEXT] = COL_TEXT;
    table[NK_COLOR_WINDOW] = COL_BG;
    table[NK_COLOR_HEADER] = COL_BG_SOFT;
    table[NK_COLOR_BORDER] = COL_BORDER;

    table[NK_COLOR_BUTTON] = COL_BG_SOFT;
    table[NK_COLOR_BUTTON_HOVER] = COL_BG_HOVER;
    table[NK_COLOR_BUTTON_ACTIVE] = COL_ACCENT;

    table[NK_COLOR_TOGGLE] = COL_BG_SOFT;
    table[NK_COLOR_TOGGLE_HOVER] = COL_BG_HOVER;
    table[NK_COLOR_TOGGLE_CURSOR] = COL_ACCENT;

    table[NK_COLOR_SELECT] = COL_BG_SOFT;
    table[NK_COLOR_SELECT_ACTIVE] = COL_ACCENT;

    table[NK_COLOR_SLIDER] = COL_BG_SOFT;
    table[NK_COLOR_SLIDER_CURSOR] = COL_ACCENT_SOFT;
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = COL_ACCENT;
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = COL_ACCENT;

    table[NK_COLOR_PROPERTY] = COL_BG_SOFT;
    table[NK_COLOR_EDIT] = COL_BG_SOFT;
    table[NK_COLOR_EDIT_CURSOR] = COL_ACCENT;

    table[NK_COLOR_COMBO] = COL_BG_SOFT;

    table[NK_COLOR_CHART] = COL_BG_SOFT;
    table[NK_COLOR_CHART_COLOR] = COL_ACCENT;
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = COL_ACCENT;

    table[NK_COLOR_SCROLLBAR] = COL_BG_SOFT;
    table[NK_COLOR_SCROLLBAR_CURSOR] = COL_BG_HOVER;
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = COL_ACCENT_SOFT;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = COL_ACCENT;

    table[NK_COLOR_TAB_HEADER] = COL_BG_SOFT;

    table[NK_COLOR_KNOB] = COL_BG_SOFT;
    table[NK_COLOR_KNOB_CURSOR] = COL_ACCENT_SOFT;
    table[NK_COLOR_KNOB_CURSOR_HOVER] = COL_ACCENT;
    table[NK_COLOR_KNOB_CURSOR_ACTIVE] = COL_ACCENT;

    nk_style_from_table(ctx, table);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    AllocConsole();

    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "r", stdin);

    HWND hWnd = NULL;
    HMENU hMenu = NULL;
    HMODULE libModule = NULL;
    HWINEVENTHOOK hWinEventHook = NULL;

    bool wndClassRegistered = false;
    bool shellIconNotified = false;

    IDirect3DDevice9Ex *device = NULL;
    struct nk_context* ctx = NULL;
    
    int result = 0;

    int bId = MessageBoxW(
        NULL,
        L"Overlap is currently in ALPHA release.\n\n"
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
        L"Any future updates must be manually downloaded from the Overlap GitHub repository:\n"
        L"https://github.com/nedpranson/overlap\n\n"
        L"Click OK to proceed.",
        L"Overlap – Update Notice",
        MB_ICONINFORMATION | MB_OK);

    libModule = LoadLibraryA("overlap.dll");
    if (!libModule) {
        MessageBoxW(
            NULL,
            L"Ensure `overlap.dll` exists in the launcher's root directory.\n\n"
            L"Otherwise, download it from the Overlap GitHub repository:\n"
            L"https://github.com/nedpranson/overlap/releases/latest",
            L"Overlap – Failed to Load `overlap.dll`.",
            MB_ICONERROR | MB_OK);

        return 1;
    }

    FARPROC proc = GetProcAddress(libModule, "__overlap_hook_proc");
    CONTINUE_IF(proc);

    HICON hIcon = LoadImageW(
        NULL,
        (LPWSTR)IDI_APPLICATION,
        IMAGE_ICON,
        32,
        32,
        LR_SHARED);

    CONTINUE_IF(hIcon);

    WNDCLASSW wndClass = {0};
    wndClass.style = CS_DBLCLKS;
    wndClass.lpfnWndProc = WndProc;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = hIcon;
    wndClass.lpszClassName = L"OverlapLauncherClass";

    CONTINUE_IF(RegisterClassW(&wndClass));
    wndClassRegistered = true;

    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    CONTINUE_IF(AdjustWindowRectEx(
        &rect,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        FALSE,
        WS_EX_APPWINDOW));

    hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wndClass.lpszClassName,
        L"Overlap Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        hInstance,
        NULL);

    CONTINUE_IF(hWnd);

    hMenu = CreatePopupMenu();
    CONTINUE_IF(hMenu);

    AppendMenuW(hMenu, MF_STRING, TRAY_TITLE, L"Overlap");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, TRAY_EXIT, L"Exit");

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)hMenu);

    NOTIFYICONDATAW data = {0};
    data.cbSize = sizeof(data);
    data.hWnd = hWnd;
    data.uID = 1;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    data.hIcon = hIcon;
    data.uCallbackMessage = WM_TRAYICON;
    lstrcpyW(data.szTip, L"Overlap");

    CONTINUE_IF(Shell_NotifyIconW(NIM_ADD, &data));
    shellIconNotified = true;

    // Smth is off with this cb func!
    //hWinEventHook = SetWinEventHook(
        //EVENT_OBJECT_FOCUS,
        //EVENT_OBJECT_FOCUS,
        //NULL,
        //WinEventProc,
        //0,
        //0,
        //WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    //CONTINUE_IF(hWinEventHook);

    EnumWindows(EnumWindowsProc, 0);

    CONTINUE_IF(SUCCEEDED(CreateD3D9Device(hWnd, &device)));

    //HHOOK hook = SetWindowsHookEx(WH_CBT, (HOOKPROC)proc, libModule, 0);
    //if (!hook) {
        //Shell_NotifyIconW(NIM_DELETE, &data);
        //DestroyMenu(hMenu);
        //FreeLibrary(libModule);
        //return 1;
    //}

    ctx = nk_d3d9_init((IDirect3DDevice9*)device, WINDOW_WIDTH, WINDOW_HEIGHT);

    struct nk_font_atlas *atlas;
    nk_d3d9_font_stash_begin(&atlas);
    nk_d3d9_font_stash_end();

    SetDarkTheme(ctx);

    while (true) {
        MSG msg;
        HRESULT hr;

        nk_input_begin(ctx);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                goto cleanup;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        nk_input_end(ctx);

        if (nk_begin(ctx, "Main", nk_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), NK_WINDOW_BORDER)) {
            //nk_layout_row_dynamic(ctx, 20, 1);
            //for (int i = 0; i < hmlen(hookedProcessesMap); i++) {
                //nk_label(ctx, hookedProcessesMap[i].value.title, NK_TEXT_LEFT);
            //}
        }
        nk_end(ctx);

        hr = RenderD3D9Objects(device);
        CONTINUE_IF(SUCCEEDED(hr));

        if (hr == S_PRESENT_OCCLUDED) {
            Sleep(10);
        }
    }

cleanup:

    //UnhookWindowsHookEx(hook);
    if (ctx) nk_d3d9_shutdown();
    if (device) IDirect3DDevice9Ex_Release(device);
    if (hWinEventHook) UnhookWinEvent(hWinEventHook);
    if (shellIconNotified) Shell_NotifyIconW(NIM_DELETE, &data);
    if (hMenu) DestroyMenu(hMenu);
    if (hWnd) DestroyWindow(hWnd);
    if (wndClassRegistered) UnregisterClassW(wndClass.lpszClassName, wndClass.hInstance);
    if (libModule) FreeLibrary(libModule);

    FreeConsole();

    return result;
}
