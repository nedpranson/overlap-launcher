#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 800

#define WM_TRAYICON (WM_USER + 1)

#define TRAY_TITLE 1
#define TRAY_EXIT 2

__declspec(dllexport) void __overlap_ignore_proc(void) {}

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

static HRESULT CreateD3D9Device(HWND hWnd, IDirect3DDevice9Ex** device) {
    HRESULT hr;

    D3DPRESENT_PARAMETERS params = { 0 };
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    params.BackBufferHeight = WINDOW_WIDTH;
    params.BackBufferHeight = WINDOW_HEIGHT;
    params.BackBufferFormat = D3DFMT_X8B8G8R8;
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
    if (!SUCCEEDED(hr)) {
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

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

    HMODULE libModule = LoadLibraryA("overlap.dll");
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
    if (!proc) {
        FreeLibrary(libModule);
        return 1;
    }

    WNDCLASSW wndClass = {0};
    wndClass.lpfnWndProc = WndProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = L"OverlapTrayClass";

    // todo: unregister class
    if (!RegisterClassW(&wndClass)) {
        FreeLibrary(libModule);
        return 1;
    };

    HWND hWnd = CreateWindowW(
        wndClass.lpszClassName,
        L"Overlap Tray Window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!hWnd) {
        FreeLibrary(libModule);
        return 1;
    }
    IDirect3DDevice9Ex *device;
    if (!SUCCEEDED(CreateD3D9Device(hWnd, &device))) {
        DestroyWindow(hWnd);
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
        IDirect3DDevice9Ex_Release(device);
        DestroyWindow(hWnd);
        FreeLibrary(libModule);
        return 1;
    }

    HMENU hMenu = CreatePopupMenu();
    if (hMenu == NULL) {
        IDirect3DDevice9Ex_Release(device);
        DestroyWindow(hWnd);
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
        IDirect3DDevice9Ex_Release(device);
        DestroyWindow(hWnd);
        FreeLibrary(libModule);
        return 1;
    }

    //HHOOK hook = SetWindowsHookEx(WH_CBT, (HOOKPROC)proc, libModule, 0);
    //if (!hook) {
        //Shell_NotifyIconW(NIM_DELETE, &data);
        //DestroyMenu(hMenu);
        //FreeLibrary(libModule);
        //return 1;
    //}

    // todo: QueryInterface
    struct nk_context* ctx = nk_d3d9_init((IDirect3DDevice9*)device, WINDOW_WIDTH, WINDOW_HEIGHT);

    if (!ctx) {
        DestroyMenu(hMenu);
        IDirect3DDevice9Ex_Release(device);
        DestroyWindow(hWnd);
        Shell_NotifyIconW(NIM_DELETE, &data);
        FreeLibrary(libModule);
    }

    struct nk_font_atlas *atlas;
    nk_d3d9_font_stash_begin(&atlas);
    nk_d3d9_font_stash_end();

    while (1) {
        MSG msg;
        HRESULT hr;

        nk_input_begin(ctx);
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        nk_input_end(ctx);

        
        if (nk_begin(
            ctx, "Demo",
            nk_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT),
            NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {

            nk_layout_row_static(ctx, 30, 80, 1);
            nk_button_label(ctx, "button");

        }
        nk_end(ctx);

        // todo: remove these asserts!!!
        // todo: add like cleanup goto
        hr = IDirect3DDevice9Ex_Clear(
            device,
            0,
            NULL,
            D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
            D3DCOLOR_COLORVALUE(0.10f, 0.18f, 0.24f, 1.0f), 0.0f, 0);
        NK_ASSERT(SUCCEEDED(hr));

        hr = IDirect3DDevice9Ex_BeginScene(device);
        NK_ASSERT(SUCCEEDED(hr));
        nk_d3d9_render(NK_ANTI_ALIASING_ON);
        hr = IDirect3DDevice9Ex_EndScene(device);
        NK_ASSERT(SUCCEEDED(hr));

        hr = IDirect3DDevice9Ex_PresentEx(device, NULL, NULL, NULL, NULL, 0);
        if (hr == S_PRESENT_OCCLUDED) {
            Sleep(10);
        }
        NK_ASSERT(SUCCEEDED(hr));
    }

    //UnhookWindowsHookEx(hook);
    nk_d3d9_shutdown();
    DestroyMenu(hMenu);
    IDirect3DDevice9Ex_Release(device);
    DestroyWindow(hWnd);
    Shell_NotifyIconW(NIM_DELETE, &data);
    FreeLibrary(libModule);

    return 0;
}
