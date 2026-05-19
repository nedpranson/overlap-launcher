#include <stdio.h>
#include <stdlib.h>
#define UNICODE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <pathcch.h>
#include <shellapi.h>
#include <d3d9.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"

#define WM_TRAYICON   (WM_USER + 1)
#define WM_HOOKNOTIFY (WM_USER + 2)

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

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

typedef struct {
    float    pos[3];
    D3DCOLOR col;
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)

IDirect3DVertexBuffer9* g_vb;
IDirect3DIndexBuffer9*  g_ib;

// impl from https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_dx9.cpp
static HRESULT create_d3d9_device_objects(IDirect3DDevice9* device) {
    HRESULT result;

    result = IDirect3DDevice9_CreateVertexBuffer(
        device,
        256 * sizeof(CUSTOMVERTEX),
        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
        D3DFVF_CUSTOMVERTEX,
        D3DPOOL_DEFAULT,
        &g_vb,
        NULL
    );

    if (FAILED(result)) {
        return result;
    }

    result = IDirect3DDevice9_CreateIndexBuffer(
        device,
        64 * sizeof(uint16_t),
        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
        D3DFMT_INDEX16,
        D3DPOOL_DEFAULT,
        &g_ib,
        NULL
    );

    if (FAILED(result)) {
        IDirect3DVertexBuffer9_Release(g_vb);
        return result;
    }

    return S_OK;
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

static HRESULT render_d3d9_objects(IDirect3DDevice9* device, Clay_RenderCommandArray commands) {
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

    CUSTOMVERTEX* vtx;
    uint16_t* idx;

    hr = IDirect3DVertexBuffer9_Lock(g_vb, 0, 256 * sizeof(CUSTOMVERTEX), (void**)&vtx, D3DLOCK_DISCARD);
    if (FAILED(hr)) {
        return hr;
    }

    hr = IDirect3DIndexBuffer9_Lock(g_ib, 0, 64 * sizeof(uint16_t), (void**)&idx, D3DLOCK_DISCARD);
    if (FAILED(hr)) {
        return hr;
    }

    for (int i = 1; i < commands.length; i++) {
        Clay_RenderCommand* cmd = &commands.internalArray[i];
        Clay_BoundingBox bbox = cmd->boundingBox;

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData* rect = &cmd->renderData.rectangle;

                printf("%f\n", rect->backgroundColor.g);

                D3DCOLOR col = D3DCOLOR_ARGB(
                    (DWORD)rect->backgroundColor.a,
                    (DWORD)rect->backgroundColor.r,
                    (DWORD)rect->backgroundColor.g,
                    (DWORD)rect->backgroundColor.b
                );

                *vtx++ = (CUSTOMVERTEX) { { bbox.x,              bbox.y,  0.0f }, col };
                *vtx++ = (CUSTOMVERTEX) { { bbox.x + bbox.width, bbox.y,  0.0f }, col };
                *vtx++ = (CUSTOMVERTEX) { { bbox.x + bbox.width, bbox.y + bbox.height, 0.0f }, col };
                *vtx++ = (CUSTOMVERTEX) { { bbox.x,              bbox.y + bbox.height, 0.0f }, col };

                *idx++ = 0; *idx++ = 1; *idx++ = 2;
                *idx++ = 0; *idx++ = 2; *idx++ = 3;

                break;
            }
            default: {
                // ... Implement handling of other command types
            }
        }
    }

    IDirect3DVertexBuffer9_Unlock(g_vb);
    IDirect3DIndexBuffer9_Unlock(g_ib);

    IDirect3DDevice9_SetStreamSource(device, 0, g_vb, 0, sizeof(CUSTOMVERTEX));
    IDirect3DDevice9_SetIndices(device, g_ib);
    IDirect3DDevice9_SetFVF(device, D3DFVF_CUSTOMVERTEX);

    D3DVIEWPORT9 vp;
    vp.X = vp.Y = 0;
    vp.Width = WINDOW_WIDTH;
    vp.Height = WINDOW_HEIGHT;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    IDirect3DDevice9_SetViewport(device, &vp);

    IDirect3DDevice9_SetPixelShader(device, NULL);
    IDirect3DDevice9_SetVertexShader(device, NULL);

    IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(device, D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(device, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_SCISSORTESTENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetTexture(device, 0, NULL);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    {
        float L = 0;
        float R = WINDOW_WIDTH + 0;
        float T = 0;
        float B = WINDOW_HEIGHT + 0;
        D3DMATRIX mat_identity = { { { 
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        } } };
        D3DMATRIX mat_projection =
        { { {
            2.0f/(R-L),   0.0f,         0.0f,  0.0f,
            0.0f,         2.0f/(T-B),   0.0f,  0.0f,
            0.0f,         0.0f,         0.5f,  0.0f,
            (L+R)/(L-R),  (T+B)/(B-T),  0.5f,  1.0f
        } } };
        IDirect3DDevice9_SetTransform(device, D3DTS_WORLD, &mat_identity);
        IDirect3DDevice9_SetTransform(device, D3DTS_VIEW, &mat_identity);
        IDirect3DDevice9_SetTransform(device, D3DTS_PROJECTION, &mat_projection);
    }

    IDirect3DDevice9_DrawIndexedPrimitive(device, D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);

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

// static DWORD get_app_data_dir_w(wchar_t* buf, size_t buf_len, const wchar_t* appname) {
//     // returns by default len excluding the null terminator
//     // except if buf is too small, returns needed len including null terminator
//     DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, buf_len);
//     DWORD appname_len;
//
//     // had space to put env var
//     bool flag = len < buf_len;
//     if (len > 0 && appname && (appname_len = lstrlenW(appname)) > 0) {
//         DWORD env_len = len;
//         len += appname_len;
//
//         if (len >= buf_len) {
//             return len + flag;
//         }
//
//         memcpy(buf + env_len, appname, appname_len * sizeof(wchar_t));
//         buf[len] = L'\0';
//     }
//
//     return len;
// }

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

const Clay_Color COLOR_LIGHT = (Clay_Color) {224, 215, 210, 255};
const Clay_Color COLOR_RED = (Clay_Color) {168, 66, 28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};

void HandleClayErrors(Clay_ErrorData errorData) {
    // See the Clay_ErrorData struct for more information
    printf("%s", errorData.errorText.chars);
    // switch(errorData.errorType) {
    //     // etc
    // }
}

// Example measure text function
// static inline Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, uintptr_t userData) {
//     (void)userData;
//     // Clay_TextElementConfig contains members such as fontId, fontSize, letterSpacing etc
//     // Note: Clay_String->chars is not guaranteed to be null terminated
//     return (Clay_Dimensions) {
//             .width = text.length * config->fontSize, // <- this will only work for monospace fonts, see the renderers/ directory for more advanced text measurement
//             .height = config->fontSize
//     };
// }

// Layout config is just a struct that can be declared statically, or inline
Clay_ElementDeclaration sidebarItemConfig = (Clay_ElementDeclaration) {
    .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50) }
    },
    .backgroundColor = COLOR_ORANGE
};

// Re-useable components are just normal functions
void SidebarItemComponent() {
    CLAY(sidebarItemConfig) {
        // children go here...
    }
}

int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PSTR /*pCmdLine*/, int /*nCmdShow*/) {
    //LPWSTR cause = NULL;
    DWORD err = S_OK;

    HICON icon = NULL;
    HWND wnd = NULL;

    IDirect3DDevice9 *device = NULL;

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

    if (FAILED(hr = create_d3d9_device_objects(device))) {
        err = HRESULT_CODE(hr);
        goto cleanup;
    }

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);

    // wchar_t hookx64_path[MAX_PATH];
    // wchar_t cmd_line[] = L"hookx64.exe overlayx64.dll";

    // if (get_app_data_dir_w(hookx64_path, MAX_PATH, L"\\Overlap\\hookx64.exe") > MAX_PATH) {
    //     err = ERROR_INSUFFICIENT_BUFFER;
    //     goto cleanup;
    // }

    // if (!CreateProcessW(
    //     hookx64_path,
    //     cmd_line,
    //     NULL,
    //     NULL,
    //     FALSE,
    //     0,
    //     NULL,
    //     NULL,
    //     &si,
    //     &hookx64_pi)) {
    //
    //     err = GetLastError();
    //     goto cleanup;
    // }

    // tood: check for alloc failure
    size_t mem_size = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc(mem_size));

    Clay_Initialize(arena, (Clay_Dimensions) { WINDOW_WIDTH, WINDOW_HEIGHT }, (Clay_ErrorHandler) { HandleClayErrors, NULL });

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        Clay_SetLayoutDimensions((Clay_Dimensions) { WINDOW_WIDTH, WINDOW_HEIGHT });
        Clay_SetPointerState((Clay_Vector2) { 0, 0 }, false);
        Clay_UpdateScrollContainers(true, (Clay_Vector2) { 0, 0 }, 16.6f);

        Clay_BeginLayout();

        // An example of laying out a UI with a fixed width sidebar and flexible width main content
        CLAY({ .id = CLAY_ID("OuterContainer"), .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 }, .backgroundColor = {250,250,255,255} }) {
            CLAY({
                .id = CLAY_ID("SideBar"),
                .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
                .backgroundColor = COLOR_LIGHT
            }) {
                // CLAY({ .id = CLAY_ID("ProfilePictureOuter"), .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = COLOR_RED }) {
                //     CLAY({ .id = CLAY_ID("ProfilePicture"), .layout = { .sizing = { .width = CLAY_SIZING_FIXED(60), .height = CLAY_SIZING_FIXED(60) }}, .image = { .imageData = &profilePicture } }) {}
                //     CLAY_TEXT(CLAY_STRING("Clay - UI Library"), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255} }));
                // }

                // Standard C code like loops etc work inside components
                for (int i = 0; i < 5; i++) {
                    SidebarItemComponent();
                }

                CLAY({ .id = CLAY_ID("MainContent"), .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } }, .backgroundColor = COLOR_LIGHT }) {}
            }
        }

        Clay_RenderCommandArray render_commands = Clay_EndLayout();

        if (FAILED(hr = render_d3d9_objects(device, render_commands))) {
            err = HRESULT_CODE(hr);
            goto cleanup;
        }
    }

cleanup:

    // todo: we can even add metadata like: failed to load icon\n{err}
    //       add some error_obj{ why: str, code: win_err }
    if (err != S_OK) display_error(err);
    if (hookx64_pi.hProcess) {
        PostThreadMessage(hookx64_pi.dwThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(hookx64_pi.hProcess, INFINITE);

        CloseHandle(hookx64_pi.hThread);
        CloseHandle(hookx64_pi.hProcess);
    }

    if (device) IDirect3DDevice9_Release(device);
    if (wnd) destroy_app_window(wnd, hInstance);
    if (icon) DestroyIcon(icon);

    return err == S_OK;
}
