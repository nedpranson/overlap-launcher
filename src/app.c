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

#define WM_TRAYICON   (WM_USER + 1)
#define WM_HOOKNOTIFY (WM_USER + 2)

#define WINDOW_CLASSNAME "OverlapLauncherClass"

#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 400

#define TRAY_TITLE 1
#define TRAY_EXIT 2

static struct {
    DWORD key;
    char*  value;
}* hooked_procs = NULL;

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
    case WM_HOOKNOTIFY:{
        DWORD pid = wParam;
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (proc == NULL) {
            break;
        }

        DWORD len;
        QueryFullProcessImageNameA(proc, 0, NULL, &len);

        if (len == 0) {
            break;
        }

        char* proc_path = malloc(len * sizeof(char));
        if (!QueryFullProcessImageNameA(proc, 0, proc_path, &len)) {
            free(proc_path);
            break;
        }

        hmput(hooked_procs, pid, proc_path);
        break;}
    case WM_COPYDATA:
    {
        PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
        char* msg = (char*)pcds->lpData;
        DWORD msg_len = pcds->cbData;

        if (msg_len > 0 && msg[msg_len - 1] == '\0') {
            OutputDebugStringA(msg);
        }

        break;
    }
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

static DWORD get_app_data_dir_w(wchar_t* buf, size_t buf_len, const wchar_t* appname) {
    // returns by default len excluding the null terminator
    // except if buf is too small, returns needed len including null terminator
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, buf_len);
    DWORD appname_len;

    // had space to put env var
    bool flag = len < buf_len;
    if (len > 0 && appname && (appname_len = lstrlenW(appname)) > 0) {
        DWORD env_len = len;
        len += appname_len;

        if (len >= buf_len) {
            return len + flag;
        }

        memcpy(buf + env_len, appname, appname_len * sizeof(wchar_t));
        buf[len] = L'\0';
    }
    
    return len;
}

static void display_error(DWORD win32_err) {
    LPWSTR error;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS | 
        FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        win32_err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&error,
        0,
        NULL
    );

    if (len == 0) {
        error = L"Unexpected Error";
    }

    MessageBoxW(NULL, error, L"Overlap - Error", MB_ICONERROR | MB_OK);

    if (len > 0) LocalFree(error);
}

int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PSTR /*pCmdLine*/, int /*nCmdShow*/) {
    //LPWSTR cause = NULL;
    DWORD err = S_OK;

    HICON icon = NULL;
    HWND wnd = NULL;

    IDirect3DDevice9 *device = NULL;
    struct nk_context* nk_ctx = NULL;

    PROCESS_INFORMATION hookx64_pi = {0};

    HRESULT hr;

    icon = LoadImage(
        NULL,
        IDI_APPLICATION,
        IMAGE_ICON,
        32,
        32,
        LR_SHARED);
    if (!icon) {
        err = GetLastError();
        goto cleanup;
    }

    wnd = create_app_window(hInstance, icon);
    if (!wnd) {
        err = GetLastError();
        goto cleanup;
    }

    if (FAILED(hr = create_d3d9_device(wnd, &device))) {
        err = HRESULT_CODE(hr);
        goto cleanup;
    }

    nk_ctx = nk_d3d9_init(device, WINDOW_WIDTH, WINDOW_HEIGHT);

    // todo: add a custom font renderer
    //       with glyph fallbacks and all
    struct nk_font_atlas *atlas;
    nk_d3d9_font_stash_begin(&atlas);
    nk_d3d9_font_stash_end();

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);

    wchar_t hookx64_path[MAX_PATH];
    wchar_t cmd_line[] = L"hookx64.exe overlayx64.dll";

    if (get_app_data_dir_w(hookx64_path, MAX_PATH, L"\\Overlap\\hookx64.exe") > MAX_PATH) {
        err = ERROR_INSUFFICIENT_BUFFER;
        goto cleanup;
    }

    if (!CreateProcessW(
        hookx64_path,
        cmd_line,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &hookx64_pi)) {

        err = GetLastError();
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
            nk_layout_row_dynamic(nk_ctx, 15.0, 3);

            int len = hmlen(hooked_procs);
            for (int i = 0; i < len; i++) {
                nk_layout_row_template_begin(nk_ctx, 20.0);
                nk_layout_row_template_push_static(nk_ctx, 16.0);
                nk_layout_row_template_push_dynamic(nk_ctx);
                nk_layout_row_template_end(nk_ctx);

                nk_label(nk_ctx, "*", NK_TEXT_CENTERED);
                nk_label(nk_ctx, hooked_procs[i].value, NK_TEXT_LEFT);

                if (len == i + 1) {
                    continue;
                }

                nk_layout_row_dynamic(nk_ctx, 1.0f, 1);
                nk_rule_horizontal(nk_ctx, nk_ctx->style.text.color, nk_false);
            }
        }
        nk_end(nk_ctx);

        if (FAILED(hr = render_d3d9_objects(device))) {
            err = HRESULT_CODE(hr);
            goto cleanup;
        }
    }

cleanup:
    for (int i = 0; i < hmlen(hooked_procs); i++) {
        free(hooked_procs[i].value);
    }
    hmfree(hooked_procs);

    // todo: we can even add metadata like: failed to load icon\n{err}
    //       add some error_obj{ why: str, code: win_err }
    if (err != S_OK) display_error(err);
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

    return err == S_OK;
}
