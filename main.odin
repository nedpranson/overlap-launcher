package main

import "base:runtime"
import "core:fmt"
import "core:c"
import win32 "core:sys/windows"

import "vendor:directx/d3d11"
import "vendor:directx/dxgi"
import d3d "vendor:directx/d3d_compiler"

import "clay"

CLASS_NAME :: "OverlapLauncherClass"

WIDTH :: 300 
HEIGHT :: 400 

main :: proc() {
    instance := win32.HINSTANCE(win32.GetModuleHandleW(nil))

    if win32.RegisterClassW(&{
        style         = win32.CS_DBLCLKS | win32.CS_HREDRAW | win32.CS_VREDRAW,
        lpfnWndProc   = WndProc,
        hInstance     = instance,
        lpszClassName = CLASS_NAME,
    }) == 0 {
        fmt.eprintln("win32::RegisterClassW failed:", win32.GetLastError())
        return
    }
    defer win32.UnregisterClassW(CLASS_NAME, instance)

    rect := win32.RECT{ 0, 0, WIDTH, HEIGHT }
    style := win32.WS_OVERLAPPED | win32.WS_CAPTION | win32.WS_SYSMENU | win32.WS_MINIMIZEBOX

    win32.AdjustWindowRectEx(&rect, style, win32.FALSE, win32.WS_EX_APPWINDOW)

    hwnd := win32.CreateWindowExW(
        win32.WS_EX_APPWINDOW,
        CLASS_NAME,
        "Overlap Launcher",
        style,
        win32.CW_USEDEFAULT,
        win32.CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
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

    // tood: use FLIP_DISCARD with 2 buffers
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
            BufferUsage = {.RENDER_TARGET_OUTPUT},
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
        fmt.eprintln("d3d11::CreateDeviceAndSwapChain failed:", hr)
        return
    }

    defer device_context->Release()
    defer device->Release()
    defer swap_chain->Release()

    back_buf: ^d3d11.ITexture2D
    hr = swap_chain->GetBuffer(0, d3d11.ITexture2D_UUID, cast(^rawptr)&back_buf)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::ISwapChain::GetBuffer failed:", hr)
        return;
    }
    defer back_buf->Release()

    rtv: ^d3d11.IRenderTargetView
	hr = device->CreateRenderTargetView(back_buf, nil, &rtv)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateRenderTargetView failed:", hr)
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
    hr = device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nil, &vs)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateVertexShader failed:", hr)
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
    hr = device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nil, &ps)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreatePixelShader failed:", hr)
        return;
    }
    defer ps->Release()

    input := [?]d3d11.INPUT_ELEMENT_DESC{
        { "POS", 0, .R32G32_FLOAT,   0,                            0, .VERTEX_DATA, 0 },
        { "TEX", 0, .R32G32_FLOAT,   0, d3d11.APPEND_ALIGNED_ELEMENT, .VERTEX_DATA, 0 },
        { "COL", 0, .R8G8B8A8_UNORM, 0, d3d11.APPEND_ALIGNED_ELEMENT, .VERTEX_DATA, 0 },
    }

    layout: ^d3d11.IInputLayout
    hr = device->CreateInputLayout(&input[0], len(input), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &layout)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateInputLayout failed:", hr)
        return;
    }
    defer layout->Release()

    blend_state: ^d3d11.IBlendState
    hr = device->CreateBlendState(&{
        AlphaToCoverageEnable = true,
        RenderTarget = { 0 = {
            BlendEnable = true,
            SrcBlend = .SRC_ALPHA,
            DestBlend = .INV_SRC_ALPHA,
            BlendOp = .ADD,
            SrcBlendAlpha = .ONE,
            DestBlendAlpha = .INV_SRC_ALPHA,
            BlendOpAlpha = .ADD,
            RenderTargetWriteMask = u8(d3d11.COLOR_WRITE_ENABLE_ALL),
        } },
    }, &blend_state)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateBlendState failed:", hr)
        return;
    }
    defer blend_state->Release()

    rasterizer: ^d3d11.IRasterizerState
    hr = device->CreateRasterizerState(&{ FillMode = .SOLID, CullMode = .NONE }, &rasterizer)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateRasterizerState failed:", hr)
        return;
    }
    defer rasterizer->Release()

    const_buf: ^d3d11.IBuffer
    hr = device->CreateBuffer(&{
        Usage = .DYNAMIC,
        CPUAccessFlags = {.WRITE},
        ByteWidth = size_of(Contants),
        BindFlags = {.CONSTANT_BUFFER},
    }, nil, &const_buf)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateBuffer failed:", hr)
        return
    }
    defer const_buf->Release()

    vertex_buf: ^d3d11.IBuffer
    hr = device->CreateBuffer(&{
        Usage = .DYNAMIC,
        CPUAccessFlags = {.WRITE},
        ByteWidth = 12 * size_of(Vertex),
        BindFlags = {.VERTEX_BUFFER},
    }, nil, &vertex_buf)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateBuffer failed:", hr)
        return
    }
    defer vertex_buf->Release()

    {
        resource: d3d11.MAPPED_SUBRESOURCE

        hr = device_context->Map(const_buf, 0, .WRITE_DISCARD, {}, &resource)
        if (win32.FAILED(hr)) {
            fmt.eprintln("d3d11::IDeviceContext::Map failed:", hr)
            return
        }
        defer device_context->Unmap(const_buf, 0)

        contants := cast(^Contants)resource.pData
        contants.mvp = {
            2.0 / WIDTH,   0.0,             0.0,   -1.0,
            0.0,           -2.0 / HEIGHT,   0.0,   1.0,
            0.0,           0.0,             0.5,   0.5,
            0.0,           0.0,             0.0,   1.0,
        }
    }

    device_context->OMSetRenderTargets(1, &rtv, nil);
    device_context->OMSetBlendState(blend_state, &[4]f32{ 0.0, 0.0, 0.0, 0.0 }, 0xFFFFFFFF);

    device_context->RSSetViewports(1, &d3d11.VIEWPORT{ 0, 0, WIDTH, HEIGHT, 0.0, 1.0 })
    device_context->RSSetState(rasterizer)
    device_context->IASetInputLayout(layout)
    device_context->VSSetConstantBuffers(0, 1, &const_buf)
    device_context->IASetPrimitiveTopology(.TRIANGLELIST)
    device_context->VSSetShader(vs, nil, 0)
    device_context->PSSetShader(ps, nil, 0)

    llen := c.size_t(clay.MinMemorySize())
    bytes := make([^]byte, llen)
    defer free(bytes)

    arena := clay.CreateArenaWithCapacityAndMemory(llen, bytes)
    clay.Initialize(arena, { WIDTH, HEIGHT }, { handler = handle_clay_errors })

    win32.ShowWindow(hwnd, win32.SW_SHOW)

    msg: win32.MSG
    for win32.GetMessageW(&msg, nil, 0, 0) > 0 {
        win32.TranslateMessage(&msg)
        win32.DispatchMessageW(&msg)
    
        clay.BeginLayout()

        if clay.UI()({
            id = clay.ID("root"),
            layout = {
                sizing = {
                    width = clay.SizingGrow({}),
                    height = clay.SizingGrow({}),
                },
                padding = clay.PaddingAll(16),
            },
            backgroundColor = { 100, 100, 100, 255 },
        }) {
            if clay.UI()({
                id = clay.ID("main"),
                layout = {
                    sizing = {
                        width = clay.SizingGrow({}),
                        height = clay.SizingGrow({}),
                    },
                },
                backgroundColor = { 224, 215, 210, 255 },
            }) {}
        }

        cmds := clay.EndLayout()

        {
            resource: d3d11.MAPPED_SUBRESOURCE

            hr = device_context->Map(vertex_buf, 0, .WRITE_DISCARD, {}, &resource)
            if (win32.FAILED(hr)) {
                fmt.eprintln("d3d11::IDeviceContext::Map failed:", hr)
                return
            }
            defer device_context->Unmap(vertex_buf, 0)

            vertices := cast([^]Vertex)resource.pData

            // for i in 0 ..< cmds.length {
            //     cmd := clay.RenderCommandArray_Get(&cmds, i)
            //     #partial switch cmd.commandType {
            //     case .Rectangle:
            //         col := rgba(
            //             u8(cmd.renderData.rectangle.backgroundColor.r),
            //             u8(cmd.renderData.rectangle.backgroundColor.g),
            //             u8(cmd.renderData.rectangle.backgroundColor.b),
            //             u8(cmd.renderData.rectangle.backgroundColor.a),
            //         )
            //         vertices[i * 6 + 0] = { { cmd.boundingBox.x, cmd.boundingBox.y }, col }; // top-left
            //         vertices[i * 6 + 1] = { { cmd.boundingBox.x + cmd.boundingBox.width, cmd.boundingBox.y }, col }; // top-right
            //         vertices[i * 6 + 2] = { { cmd.boundingBox.x + cmd.boundingBox.width, cmd.boundingBox.y + cmd.boundingBox.height }, col }; // bottom-right
            //
            //         vertices[i * 6 + 3] = { { cmd.boundingBox.x + cmd.boundingBox.width, cmd.boundingBox.y + cmd.boundingBox.height }, col }; // bottom-right
            //         vertices[i * 6 + 4] = { { cmd.boundingBox.x, cmd.boundingBox.y + cmd.boundingBox.height }, col }; // bottom-left
            //         vertices[i * 6 + 5] = { { cmd.boundingBox.x, cmd.boundingBox.y }, col }; // top-left
            //     }
            // }

            vertices[0] = { { 25.0,  25.0  }, {0.0, 0.0}, 0xFF0000FF } // top-left
            vertices[1] = { { 275.0, 25.0  }, {1.0, 0.0}, 0xFF00FF00 } // top-right
            vertices[2] = { { 275.0, 375.0 }, {1.0, 1.0}, 0xFFFF0000 } // bottom-right

            vertices[3] = { { 275.0, 375.0 }, {1.0, 1.0}, 0xFFFF0000 } // bottom-right
            vertices[4] = { { 25.0,  375.0 }, {0.0, 1.0}, 0xFFFFFF00 } // bottom-left
            vertices[5] = { { 25.0,  25.0  }, {0.0, 0.0}, 0xFF0000FF } // top-left
            // vertices[0] = { { 150.0, 0.0 }, 0xFF0000FF }
            // vertices[1] = { { 0, 400 }, 0xFF00FF00 }
            // vertices[2] = { { 300, 400 }, 0xFFFF0000 }

            // vertices[0] = { { 150.0, 0.0 }, 0xFF0000FF }
            // vertices[1] = { { 0, 400 }, 0xFF00FF00 }
            // vertices[2] = { { 300, 400 }, 0xFFFF0000 }
        }

        device_context->ClearRenderTargetView(rtv, &[4]f32{0.25, 0.5, 1.0, 1.0})

        offset := u32(0)
        stride := u32(size_of(Vertex))

        device_context->IASetVertexBuffers(0, 1, &vertex_buf, &stride, &offset)
        device_context->Draw(6, 0)

        hr = swap_chain->Present(1, {})
        if win32.FAILED(hr) {
            fmt.eprintln("d3d11::ISwapChain::Present failed:", hr)
            return;
        }
    }
}

rgba :: proc(r, g, b, a: u8) -> u32 {
    return (u32(a) << 24) |
           (u32(r) << 16) |
           (u32(g) << 8)  |
           (u32(b) << 0)
}

Contants :: struct {
    mvp: matrix[4,4]f32
}

Vertex :: struct {
    pos: [2]f32,
    uv: [2]f32,
    col: u32,
}

handle_clay_errors :: proc "c" (error: clay.ErrorData) {
    context = runtime.default_context()
    fmt.eprintln(error)
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
    float2 uv  : TEX;
};

struct vs_out {
    float4 pos : SV_POSITION;
    float4 col : COL;
    float2 uv  : TEX;
};

const float fade = 0.006;

vs_out vs_main(vs_in input) {
    vs_out output;
    output.pos = mul(mvp, float4(input.pos, 0.0, 1.0));
    output.col = input.col;
    output.uv = input.uv;
    return output;
}

float sd_rounded_box(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float4 ps_main(vs_out input) : SV_TARGET {
    float2 uv = input.uv * 2.0 - 1.0;
    uv.x *= 250.0 / 350.0;

    float2 size = float2(250.0 / 350.0, 1.0) - (fade * 0.5);
    float4 radius = float4(0.25, 0.5, 0.75, 1.0);

    float4 radii = min(radius, min(size.x, size.y));

    float d = sd_rounded_box(uv, size, radii);
    float a = 1.0 - smoothstep(0.0, fade, d);

    float4 col = input.col;
    col.a *= a;

    return col;
}
`
