#include "minwindef.h"
#define UNICODE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <pathcch.h>
#include <shellapi.h>

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

#define WM_TRAYICON (WM_USER + 1)

#define WINDOW_CLASSNAME "OverlapLauncherClass"

#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 400

#define TRAY_TITLE 1
#define TRAY_EXIT 2

static LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT point;

            GetCursorPos(&point);
            SetForegroundWindow(hwnd);

            HMENU hMenu = (HMENU)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

            TrackPopupMenu(
                hMenu,
                TPM_RIGHTBUTTON,
                point.x,
                point.y,
                0,
                hwnd,
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

    if (nk_d3d9_handle_event(hwnd, uMsg, wParam, lParam)) {
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

static HWND create_app_window(HINSTANCE instance, HICON icon) {
    HWND wnd = NULL;
    HMENU menu = NULL;

    WNDCLASS wnd_class = {0};
    wnd_class.style = CS_DBLCLKS;
    wnd_class.lpfnWndProc = WndProc;
    wnd_class.hInstance = instance;
    wnd_class.hIcon = icon;
    wnd_class.lpszClassName = TEXT(WINDOW_CLASSNAME);

    if (!RegisterClass(&wnd_class)) {
        return NULL;
    }

    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    if (!AdjustWindowRectEx(&rect, style, FALSE, WS_EX_APPWINDOW)) {
        goto error;
    }

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
        instance,
        NULL);

    if (!wnd) {
        goto error;
    }

    menu = CreatePopupMenu();
    if (!menu) {
        goto error;
    }

    if (!AppendMenu(menu, MF_STRING, TRAY_TITLE, TEXT("Overlap"))) goto error;
    if (!AppendMenu(menu, MF_SEPARATOR, 0, NULL)) goto error;
    if (!AppendMenu(menu, MF_STRING, TRAY_EXIT, TEXT("Exit"))) goto error;

    SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)menu);

    NOTIFYICONDATA data = {0};
    data.cbSize = sizeof(data);
    data.hWnd = wnd;
    data.uID = 1;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    data.hIcon = icon;
    data.uCallbackMessage = WM_TRAYICON;
    lstrcpy(data.szTip, TEXT("Overlap"));

    if (!Shell_NotifyIcon(NIM_ADD, &data)) {
        goto error;
    }

    return wnd;
error:
    if (menu) DestroyMenu(menu);
    if (wnd) DestroyWindow(wnd);
    UnregisterClass(TEXT(WINDOW_CLASSNAME), instance);

    return NULL;
}

static void destroy_app_window(HWND wnd, HINSTANCE instance) {
    HMENU menu = (HMENU)GetWindowLongPtr(wnd, GWLP_USERDATA);

    NOTIFYICONDATA data = {0};
    data.cbSize = sizeof(data);
    data.hWnd = wnd;
    data.uID = 1;

    Shell_NotifyIcon(NIM_DELETE, &data);
    DestroyMenu(menu);
    DestroyWindow(wnd);
    UnregisterClass(TEXT(WINDOW_CLASSNAME), instance);
}

int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PSTR /*pCmdLine*/, int /*nCmdShow*/) {
    int res = 0;

    HICON icon = NULL;
    HWND wnd = NULL;

    IDirect3DDevice9 *device = NULL;
    struct nk_context* nk_ctx = NULL;

    PROCESS_INFORMATION hookx64_pi = {0};

    icon = LoadImage(
        NULL,
        IDI_APPLICATION,
        IMAGE_ICON,
        32,
        32,
        LR_SHARED);
    if (!icon) {
        res = 1;
        goto cleanup;
    }

    wnd = create_app_window(hInstance, icon);
    if (!wnd) {
        res = 1;
        goto cleanup;
    }

    if (FAILED(create_d3d9_device(wnd, &device))) {
        res = 1;
        goto cleanup;
    }

    nk_ctx = nk_d3d9_init(device, WINDOW_WIDTH, WINDOW_HEIGHT);

    // todo: add a custom font renderer
    //       with glyph fallbacks and all
    struct nk_font_atlas *atlas;
    nk_d3d9_font_stash_begin(&atlas);
    nk_d3d9_font_stash_end();

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);

    // LOCALAPPDATA/Overlap/hookx64.exe
    // LOCALAPPDATA/Overlap/overlayx64.dll
    // todo: after spawning procs from appdata
    //       make message boxes on failures
    char local_app_data_dir[MAX_PATH];
    DWORD len = GetEnvironmentVariableA(
        "LOCALAPPDATA",
        local_app_data_dir,
        MAX_PATH
    );

    if (len > 0) {
        printf("%s\n", local_app_data_dir);
    }

    //
    // 32,767

    //UNICODE_STRING_MAX_CHARS

    // on W buf can be modified
    // so for now will just use A
    if (!CreateProcessA(
        "hookx64.exe",
        "hookx64.exe overlapx64.dll",
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &hookx64_pi)) {
        res = 1;
        goto cleanup;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        nk_input_begin(nk_ctx);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        nk_input_end(nk_ctx);

        struct nk_rect bounds;
        if (nk_begin(nk_ctx, "Overlap Launcher", nk_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
            nk_layout_row_dynamic(nk_ctx, 30.0, 3);

            bounds = nk_widget_bounds(nk_ctx);
            nk_fill_rect(
                &nk_ctx->current->buffer,
                nk_rect(bounds.x, bounds.y + bounds.h, bounds.w, 1.0f),
                nk_false,
                nk_ctx->style.text.color);
            nk_button_label(nk_ctx, "Injected");

            bounds = nk_widget_bounds(nk_ctx);
            nk_fill_rect(
                &nk_ctx->current->buffer,
                nk_rect(bounds.x, bounds.y + bounds.h, bounds.w, 1.0f),
                nk_false,
                nk_ctx->style.text.color);
            nk_button_label(nk_ctx, "Excluded");

            bounds = nk_widget_bounds(nk_ctx);
            nk_fill_rect(
                &nk_ctx->current->buffer,
                nk_rect(bounds.x, bounds.y + bounds.h, bounds.w, 1.0f),
                nk_false,
                nk_ctx->style.text.color);
            nk_button_label(nk_ctx, "Settings");

            // gap!
            // nk_layout_row_dynamic(nk_ctx, 15.0, 3);

            // int len = hmlen(ctx.hk_procs_map);
            // for (int i = 0; i < len; i++) {
            //     HANDLE proc = ctx.hk_procs_map[i].proc;
            //
            //     char path[MAX_PATH];
            //     DWORD path_len = MAX_PATH;
            //
            //     if (!QueryFullProcessImageNameA(proc, 0, path, &path_len)) {
            //         continue;
            //     }
            //
            //     nk_layout_row_template_begin(nk_ctx, 20.0);
            //     nk_layout_row_template_push_static(nk_ctx, 16.0);
            //     nk_layout_row_template_push_dynamic(nk_ctx); 
            //     nk_layout_row_template_end(nk_ctx);
            //
            //     nk_label(nk_ctx, "*", NK_TEXT_CENTERED);
            //     nk_label(nk_ctx, path, NK_TEXT_LEFT);
            //
            //     if (len == i + 1) {
            //         continue;
            //     }
            //
            //     nk_layout_row_dynamic(nk_ctx, 1.0f, 1);
            //     nk_rule_horizontal(nk_ctx, nk_ctx->style.text.color, nk_false);
            // }
        }
        nk_end(nk_ctx);

        if (FAILED(render_d3d9_objects(device))) {
            res = 1;
            goto cleanup;
        }
    }

cleanup:
    if (hookx64_pi.hProcess) {
        PostThreadMessage(hookx64_pi.dwThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(hookx64_pi.hProcess, INFINITE);

        CloseHandle(hookx64_pi.hThread);
        CloseHandle(hookx64_pi.hProcess);
    }

    if (nk_ctx) nk_d3d9_shutdown();
    if (device) IDirect3DDevice9_Release(device);
    if (wnd) destroy_app_window(wnd, hInstance);
    if (icon) DestroyIcon(icon);
    return res;
}
