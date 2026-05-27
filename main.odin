package main

import "core:fmt"
import win32 "core:sys/windows"

import "vendor:directx/d3d11"
import "vendor:directx/dxgi"
import d3d "vendor:directx/d3d_compiler"

main :: proc() {
    instance := win32.HINSTANCE(win32.GetModuleHandleW(nil))

    if win32.RegisterClassW(&{
        style         = win32.CS_DBLCLKS | win32.CS_HREDRAW | win32.CS_VREDRAW,
        lpfnWndProc   = WndProc,
        hInstance     = instance,
        lpszClassName = "OverlapLauncherClass",
    }) == 0 {
        fmt.eprintln("win32::RegisterClassW failed:", win32.GetLastError())
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
        fmt.eprintln("win32::CreateWindowExW failed:", win32.GetLastError())
        return
    }

    defer win32.DestroyWindow(hwnd)

    levels := [?]d3d11.FEATURE_LEVEL{
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
        { .BGRA_SUPPORT },
        &levels[0],
        len(levels),
        d3d11.SDK_VERSION,
        &{
            BufferDesc = { Format = .R8G8B8A8_UNORM },
            SampleDesc = { Count = 1 },
            BufferCount = 1,
            OutputWindow = hwnd,
            Windowed = true,
            SwapEffect = .DISCARD,
        },
        &swap_chain,
        &device,
        nil,
        &device_context,
    )

    if win32.FAILED(hr) {
        fmt.eprintln("win32::CreateDeviceAndSwapChain failed:", hr)
        return
    }

    defer device_context->Release()
    defer device->Release()
    defer swap_chain->Release()

    back_buf: ^d3d11.ITexture2D
    hr = swap_chain->GetBuffer(0, d3d11.ITexture2D_UUID, cast(^rawptr)&back_buf)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::GetBuffer failed:", hr)
        return;
    }
    defer back_buf->Release()

    rtv: ^d3d11.IRenderTargetView
	hr = device->CreateRenderTargetView(back_buf, nil, &rtv)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::CreateRenderTargetView failed:", hr)
        return;
    }
    defer rtv->Release()

    vs_blob: ^d3d11.IBlob
    {
        err_blob: ^d3d11.IBlob
        hr := d3d.Compile(raw_data(hlsl), len(hlsl), "shader.hlsl", nil, nil, "vs_main", "vs_5_0", 0, 0, &vs_blob, &err_blob)

        if win32.FAILED(hr) {
            fmt.eprintln("d3d::Compile failed:", hr)
            fmt.eprint(blob_to_string(err_blob))

            err_blob->Release()
            return
        }
    }
    defer vs_blob->Release()

    vs: ^d3d11.IVertexShader
    hr = device->CreateVertexShader(vs_blob.GetBufferPointer(vs_blob), vs_blob.GetBufferSize(vs_blob), nil, &vs)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::CreateVertexShader failed:", hr)
        return;
    }
    defer vs->Release()

    ps_blob: ^d3d11.IBlob
    {
        err_blob: ^d3d11.IBlob
        hr := d3d.Compile(raw_data(hlsl), len(hlsl), "shader.hlsl", nil, nil, "ps_main", "ps_5_0", 0, 0, &ps_blob, &err_blob)

        if win32.FAILED(hr) {
            fmt.eprintln("d3d::Compile failed:", hr)
            fmt.eprint(blob_to_string(err_blob))

            err_blob->Release()
            return
        }
    }
    defer ps_blob->Release()

    ps: ^d3d11.IPixelShader
    hr = device->CreatePixelShader(ps_blob.GetBufferPointer(ps_blob), ps_blob.GetBufferSize(ps_blob), nil, &ps)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::CreatePixelShader failed:", hr)
        return;
    }
    defer ps->Release()

    input := [?]d3d11.INPUT_ELEMENT_DESC{
        { "POS", 0, .R32G32_FLOAT,   0,                            0, .VERTEX_DATA, 0 },
        { "COL", 0, .R8G8B8A8_UNORM, 0, d3d11.APPEND_ALIGNED_ELEMENT, .VERTEX_DATA, 0 },
    }

    layout: ^d3d11.IInputLayout
    hr = device->CreateInputLayout(&input[0], len(input), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &layout)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::CreateInputLayout failed:", hr)
        return;
    }
    defer layout->Release()

    const_buf: ^d3d11.IBuffer
    hr = device->CreateBuffer(&{
        Usage = .DYNAMIC,
        CPUAccessFlags = {.WRITE},
        ByteWidth = size_of(Contants),
        BindFlags = {.CONSTANT_BUFFER},
    }, nil, &const_buf)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::CreateBuffer failed:", hr)
        return;
    }
    defer const_buf->Release()

    win32.ShowWindow(hwnd, win32.SW_SHOW)

    msg: win32.MSG
    for win32.GetMessageW(&msg, nil, 0, 0) > 0 {
        win32.TranslateMessage(&msg)
        win32.DispatchMessageW(&msg)

        device_context->ClearRenderTargetView(rtv, &[4]f32{0.25, 0.5, 1.0, 1.0})

        hr = swap_chain->Present(1, {})
        if win32.FAILED(hr) {
            fmt.eprintln("d3d11::Present failed:", hr)
            return;
        }
    }
}

Contants :: struct #align(16) {
    mvp: matrix[4,4]f32
}

WndProc :: proc "stdcall" (hwnd: win32.HWND, msg: win32.UINT, wparam: win32.WPARAM, lparam: win32.LPARAM) -> win32.LRESULT {
    switch msg {
    case win32.WM_DESTROY:
        win32.PostQuitMessage(0);
        return 0;
    }
    return win32.DefWindowProcW(hwnd, msg, wparam, lparam)
}

blob_to_string :: proc(blob: ^d3d.ID3DBlob) -> string {
    assert(blob != nil)

    ptr := cast([^]u8)blob->GetBufferPointer()
    return string(ptr[:blob->GetBufferSize()])
}

hlsl := `
cbuffer vertexBuffer : register(b0) {
    float4x4 mvp;
}

struct vs_in {
    float2 pos : POS;
    float4 col : COL;
};

struct vs_out {
    float4 pos : SV_POSITION;
    float4 col : COL;
};

vs_out vs_main(vs_in input) {
    vs_out output;
    output.pos = mul(mvp, float4(input.pos, 0.0, 1.0));
    output.col = input.col;
    return output;
}

float4 ps_main(vs_out input) : SV_TARGET {
    return input.col;
}
`
