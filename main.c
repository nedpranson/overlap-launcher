#define UNICODE
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

#define CONTINUE_IF(ok) if (!ok) { \
    res = 1; \
    goto cleanup; \
}

#define WM_TRAYICON (WM_USER + 1)
#define WM_PROCEXITED (WM_USER + 2)

#define TRAY_TITLE 1
#define TRAY_EXIT 2

struct hk_proc_map {
    DWORD key;

    HANDLE proc;
    HANDLE exit;
};

struct ctx {
    HMENU menu;
    struct hk_proc_map* hk_procs_map;
};

struct hk_proc_ctx {
    HWND wnd;
    DWORD pid;
};

static VOID CALLBACK
ProcessExitCallback(PVOID lpParameter, BOOLEAN /*TimerOrWaitFired*/) {
    struct hk_proc_ctx* ctx = (struct hk_proc_ctx*)lpParameter;

    PostMessage(
        ctx->wnd,
        WM_PROCEXITED,
        (WPARAM)ctx->pid,
        0);

    free(ctx);
}

static void hook_process(HWND msg_wnd, DWORD pid, struct hk_proc_map** map) {
    if (hmgeti(*map, pid) >= 0) {
        return;
    }

    HANDLE proc;
    HANDLE exit;

    proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!proc) {
        return;
    }

    struct hk_proc_ctx* ctx = malloc(sizeof(struct hk_proc_ctx));
    if (!ctx) {
        CloseHandle(proc);
        return;
    }

    ctx->wnd = msg_wnd;
    ctx->pid = pid;

    if (!RegisterWaitForSingleObject(
        &exit,
        proc,
        ProcessExitCallback,
        ctx,
        INFINITE,
        WT_EXECUTEONLYONCE)) {

        free(ctx);
        CloseHandle(proc);
        return;
    }

    hmputs(*map, ((struct hk_proc_map){ pid, proc, exit }));
}

static HRESULT create_d3d9_device(HWND wnd, IDirect3DDevice9** device) {
    HRESULT hr;

    IDirect3D9Ex *d3d9;
    IDirect3DDevice9Ex* d3d9_device;

    D3DPRESENT_PARAMETERS params = { 0 };
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    params.BackBufferWidth = WINDOW_WIDTH;
    params.BackBufferHeight = WINDOW_HEIGHT;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 1;
    params.MultiSampleType = D3DMULTISAMPLE_NONE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = wnd;
    params.EnableAutoDepthStencil = TRUE;
    params.AutoDepthStencilFormat = D3DFMT_D24S8;
    params.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    params.Windowed = TRUE;

    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9);
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = IDirect3D9Ex_CreateDeviceEx(
        d3d9,
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        wnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE,
        &params,
        NULL,
        &d3d9_device);

    IDirect3D9Ex_Release(d3d9);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IDirect3DDevice9Ex_QueryInterface(
        d3d9_device,
        &IID_IDirect3DDevice9,
        (void**)device);

    IDirect3DDevice9Ex_Release(d3d9_device);
    return hr;
}

static HRESULT render_d3d9_objects(IDirect3DDevice9* device) {
    HRESULT hr;

    hr = IDirect3DDevice9_Clear(
        device,
        0,
        NULL,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
        D3DCOLOR_COLORVALUE(1.00f, 1.0f, 1.0f, 1.0f), 0.0f, 0);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IDirect3DDevice9_BeginScene(device);
    if (FAILED(hr)) {
        return hr;
    }

    nk_d3d9_render(NK_ANTI_ALIASING_ON);

    hr = IDirect3DDevice9_EndScene(device);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);
    return hr;
}

static LRESULT CALLBACK
WndProc(HWND hWnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam) {

    struct ctx* ctx = (struct ctx*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam != WM_RBUTTONUP) break;

        POINT point;

        GetCursorPos(&point);
        SetForegroundWindow(hWnd);

        TrackPopupMenu(
            ctx->menu,
            TPM_RIGHTBUTTON,
            point.x,
            point.y,
            0,
            hWnd,
            NULL);
        break;
    case WM_PROCEXITED: {
        DWORD pid = (DWORD)wParam;

        struct hk_proc_map hk_proc = hmgets(ctx->hk_procs_map, pid);
        hmdel(ctx->hk_procs_map, pid);

        UnregisterWait(hk_proc.exit);
        CloseHandle(hk_proc.proc);

        break;
    }
    case WM_TIMER: {
        HWND fg_wnd = GetForegroundWindow();
        DWORD pid;
        if (fg_wnd && GetWindowThreadProcessId(fg_wnd, &pid) > 0) {
            hook_process(hWnd, pid, &ctx->hk_procs_map);
        }

        break;
    }
    case WM_COMMAND:
        DWORD cmd = LOWORD(wParam);
        if (cmd == TRAY_EXIT) {
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
    
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY
WinMain(HINSTANCE hInstance,
        HINSTANCE /*hPrevInstance*/,
        PSTR /*pCmdLine*/,
        int /*nCmdShow*/) {

    AllocConsole();

    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "r", stdin);

    int res = 0;

    HICON icon = NULL;

    HWND wnd = NULL;
    HMENU menu = NULL;

    bool wnd_class_registered = false;
    bool shell_icon_notified = false;

    IDirect3DDevice9 *device = NULL;
    struct nk_context* nk_ctx = NULL;

    icon = LoadImage(
        NULL,
        IDI_APPLICATION,
        IMAGE_ICON,
        32,
        32,
        LR_SHARED);
    CONTINUE_IF(icon);

    WNDCLASS wnd_class = {0};
    wnd_class.style = CS_DBLCLKS;
    wnd_class.lpfnWndProc = WndProc;
    wnd_class.hInstance = hInstance;
    wnd_class.hIcon = icon;
    wnd_class.lpszClassName = TEXT("OverlapLauncherClass");

    CONTINUE_IF(RegisterClass(&wnd_class));
    wnd_class_registered = true;
    
    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    CONTINUE_IF(AdjustWindowRectEx(
        &rect,
        style,
        FALSE,
        WS_EX_APPWINDOW));

    wnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        wnd_class.lpszClassName,
        TEXT("Overlap Launcher"),
        style | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        hInstance,
        NULL);
    CONTINUE_IF(wnd);

    menu = CreatePopupMenu();
    CONTINUE_IF(menu);

    CONTINUE_IF(AppendMenu(menu, MF_STRING, TRAY_TITLE, TEXT("Overlap")));
    CONTINUE_IF(AppendMenu(menu, MF_SEPARATOR, 0, NULL));
    CONTINUE_IF(AppendMenu(menu, MF_STRING, TRAY_EXIT, TEXT("Exit")));

    NOTIFYICONDATA data = {0};
    data.cbSize = sizeof(data);
    data.hWnd = wnd;
    data.uID = 1;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    data.hIcon = icon;
    data.uCallbackMessage = WM_TRAYICON;
    lstrcpy(data.szTip, TEXT("Overlap"));

    CONTINUE_IF(Shell_NotifyIcon(NIM_ADD, &data));
    shell_icon_notified = true;

    struct ctx ctx = {0};
    ctx.menu = menu;

    SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)&ctx);

    CONTINUE_IF(SUCCEEDED(create_d3d9_device(wnd, &device)));

    nk_ctx = nk_d3d9_init(device, WINDOW_WIDTH, WINDOW_HEIGHT);

    // todo: add a custom font renderer
    //       with glyph fallbacks and all
    struct nk_font_atlas *atlas;
    nk_d3d9_font_stash_begin(&atlas);
    nk_d3d9_font_stash_end();

    // registers periodic foreground window checks
    SetTimer(wnd, 0, 1000, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        nk_input_begin(nk_ctx);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        nk_input_end(nk_ctx);

        if (nk_begin(nk_ctx, "Main", nk_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(nk_ctx, 20, 1);
            for (int i = 0; i < hmlen(ctx.hk_procs_map); i++) {
                HANDLE proc = ctx.hk_procs_map[i].proc;

                char path[MAX_PATH];
                DWORD len = MAX_PATH;

                if (!QueryFullProcessImageNameA(
                    proc,
                    0,
                    path,
                    &len)) {

                    continue;
                }

                nk_button_label(nk_ctx, path);
            }
        }
        nk_end(nk_ctx);
 
        CONTINUE_IF(SUCCEEDED(render_d3d9_objects(device)));
    }

cleanup:
    for (int i = 0; i < hmlen(ctx.hk_procs_map); i++) {
        HANDLE done = CreateEvent(NULL, TRUE, FALSE, NULL);

        UnregisterWaitEx(ctx.hk_procs_map[i].exit, done);
        WaitForSingleObject(done, INFINITE);

        CloseHandle(done);
        CloseHandle(ctx.hk_procs_map[i].proc);
    }
    hmfree(ctx.hk_procs_map);
    if (nk_ctx) nk_d3d9_shutdown();
    if (device) IDirect3DDevice9_Release(device);
    if (shell_icon_notified) Shell_NotifyIcon(NIM_DELETE, &data);
    if (menu) DestroyMenu(menu);
    if (wnd) DestroyWindow(wnd);
    if (wnd_class_registered) UnregisterClass(wnd_class.lpszClassName, hInstance);
    if (icon) DestroyIcon(icon);
    FreeConsole();

    return res;
}
