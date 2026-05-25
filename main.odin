package main

import "core:fmt"

import win32 "core:sys/windows"
import "vendor:directx/d3d11"
import "vendor:directx/dxgi"

main :: proc() {
    instance := win32.HINSTANCE(win32.GetModuleHandleW(nil))

    if win32.RegisterClassW(&{
        style         = win32.CS_DBLCLKS | win32.CS_HREDRAW | win32.CS_VREDRAW,
        lpfnWndProc   = WndProc,
        hInstance     = instance,
        lpszClassName = "OverlapLauncherClass",
    }) == 0 {
        fmt.eprintln("RegisterClassW failed:", win32.GetLastError())
        return
    }
    defer win32.UnregisterClassW("OverlapLauncherClass", instance)

    hwnd := win32.CreateWindowExW(
        win32.WS_EX_APPWINDOW,
        "OverlapLauncherClass",
        "Overlap Launcher",
        win32.WS_OVERLAPPED | win32.WS_CAPTION | win32.WS_SYSMENU | win32.WS_MINIMIZEBOX,
        win32.CW_USEDEFAULT,
        win32.CW_USEDEFAULT,
        300,
        400,
        nil,
        nil,
        instance,
        nil,
    );

    if hwnd == nil {
        fmt.eprintln("CreateWindowExW failed:", win32.GetLastError())
        return
    }

    defer win32.DestroyWindow(hwnd)

    levels := []d3d11.FEATURE_LEVEL{
        ._11_0,
        ._10_1,
        ._10_0,
    }

    swap_chain:     ^dxgi.ISwapChain
    device:         ^d3d11.IDevice
    device_context: ^d3d11.IDeviceContext

    hr := d3d11.CreateDeviceAndSwapChain(
        nil,
        .HARDWARE,
        nil,
        d3d11.CREATE_DEVICE_FLAGS{},
        &levels[0],
        u32(len(levels)),
        d3d11.SDK_VERSION,
        &{
            BufferDesc = { Format = .R8G8B8A8_UNORM },
            SampleDesc = { Count = 1 },
            BufferCount = 1,
            OutputWindow = hwnd,
            Windowed = win32.TRUE,
            SwapEffect = dxgi.SWAP_EFFECT.DISCARD,
        },
        &swap_chain,
        &device,
        nil,
        &device_context,
    )

    if win32.FAILED(hr) {
        fmt.eprintln("CreateDeviceAndSwapChain failed:", hr)
        return
    }
    
    defer swap_chain.Release(device_context)
    defer device.Release(device_context)
    defer device_context.Release(device_context)

    win32.ShowWindow(hwnd, win32.SW_SHOW)

    msg: win32.MSG;
    for win32.GetMessageW(&msg, nil, 0, 0) > 0 {
        win32.TranslateMessage(&msg);
        win32.DispatchMessageW(&msg);
    }

}

WndProc :: proc "stdcall" (hwnd: win32.HWND, msg: win32.UINT, wparam: win32.WPARAM, lparam: win32.LPARAM) -> win32.LRESULT {
    switch msg {
    case win32.WM_DESTROY:
        win32.PostQuitMessage(0);
        return 0;
    }
    return win32.DefWindowProcW(hwnd, msg, wparam, lparam)
}
