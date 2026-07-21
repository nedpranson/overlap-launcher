package main

import "core:os"
import "base:runtime"
import "core:fmt"
import "core:c"
import win32 "core:sys/windows"

import "vendor:directx/d3d11"
import "vendor:directx/dxgi"
import d3d "vendor:directx/d3d_compiler"

import "clay"
import oc "onecore"
import stbrp "vendor:stb/rect_pack"

CLASS_NAME :: "OverlapLauncherClass"

WIDTH :: 300 
HEIGHT :: 400 

face   : oc.face
glyphs : map[Font_Key]Glyph_Info
atlas  : Atlas

Atlas :: struct {
    width: uint,
    height: uint,
    ctx: stbrp.Context,
    pixels: []u8,
    nodes: []stbrp.Node,
}

Font_Key :: struct {
    charcode: rune,
    font_size: oc.i26p6,
}

Glyph_Info :: struct {
    x, y: stbrp.Coord,
    w, h: stbrp.Coord,
    metrics: oc.glyph_metrics,
}

main :: proc() {
    atlas = Atlas{
        width = 512,
        height = 512,
        pixels = make([]u8, 512 * 512),
        nodes = make([]stbrp.Node, 512),
    }
    defer delete(atlas.pixels)
    defer delete(atlas.nodes)

    glyphs = make(map[Font_Key]Glyph_Info)
    defer delete(glyphs)

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
        { "POS", 0,       .R32G32_FLOAT, 0,                            0, .VERTEX_DATA,   0 },
        { "TEX", 0,       .R32G32_FLOAT, 0, d3d11.APPEND_ALIGNED_ELEMENT, .VERTEX_DATA,   0 },
        { "OFF", 0,       .R32G32_FLOAT, 1,                            0, .INSTANCE_DATA, 1 },
        { "COL", 0,     .R8G8B8A8_UNORM, 1, d3d11.APPEND_ALIGNED_ELEMENT, .INSTANCE_DATA, 1 },
        { "RAD", 0, .R32G32B32A32_FLOAT, 1, d3d11.APPEND_ALIGNED_ELEMENT, .INSTANCE_DATA, 1 },
        { "DIM", 0,       .R32G32_FLOAT, 1, d3d11.APPEND_ALIGNED_ELEMENT, .INSTANCE_DATA, 1 },
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
    hr = device->CreateBuffer(
        &{
            ByteWidth = 6 * size_of(Vertex),
            BindFlags = {.VERTEX_BUFFER},
        },
        &{
            pSysMem = raw_data([]Vertex{
                { { 0.0, 0.0 }, { 0.0, 0.0 } },
                { { 1.0, 0.0 }, { 1.0, 0.0 } },
                { { 1.0, 1.0 }, { 1.0, 1.0 } },
                { { 1.0, 1.0 }, { 1.0, 1.0 } },
                { { 0.0, 1.0 }, { 0.0, 1.0 } },
                { { 0.0, 0.0 }, { 0.0, 0.0 } },
            }),
        },
        &vertex_buf
    )
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateBuffer failed:", hr)
        return
    }
    defer vertex_buf->Release()

    instance_buf: ^d3d11.IBuffer
    hr = device->CreateBuffer(&{
        Usage = .DYNAMIC,
        CPUAccessFlags = {.WRITE},
        ByteWidth = 256 * size_of(Instance),
        BindFlags = {.VERTEX_BUFFER},
    }, nil, &instance_buf)
    if win32.FAILED(hr) {
        fmt.eprintln("d3d11::IDevice::CreateBuffer failed:", hr)
        return
    }
    defer instance_buf->Release()

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

    lib: ^oc.library
    if err := oc.init_library(&lib); err != .ok {
        fmt.eprintln("oc::init_library failed:", err)
        return
    }
    defer oc.free_library(lib)

    if err := oc.open_face(lib, "fonts/SegoeUI-Regular.ttf", nil, &face); err != .ok {
        fmt.eprintln("oc::open_face failed:", err)
        return
    }
    defer oc.free_face(&face)

    llen := c.size_t(clay.MinMemorySize())
    bytes := make([^]byte, llen)
    defer free(bytes)

    arena := clay.CreateArenaWithCapacityAndMemory(llen, bytes)
    clay.Initialize(arena, { WIDTH, HEIGHT }, { handler = handle_clay_errors })

    clay.SetMeasureTextFunction(measure_clay_text, nil)

    stbrp.init_target(&atlas.ctx, 512, 512, raw_data(atlas.nodes), i32(len(atlas.nodes)))
    stbrp.setup_heuristic(&atlas.ctx, .Skyline_default)

    win32.ShowWindow(hwnd, win32.SW_SHOW)

    msg: win32.MSG
    for win32.GetMessageW(&msg, nil, 0, 0) > 0 {
        win32.TranslateMessage(&msg)
        win32.DispatchMessageW(&msg)
    
        clay.BeginLayout()

        if clay.UI()({
            id = clay.ID("main"),
            layout = {
                sizing = {
                    width = clay.SizingGrow({}),
                    height = clay.SizingGrow({}),
                },
                padding = clay.PaddingAll(16),
                childGap = 16,
                layoutDirection = .TopToBottom,
            },
            backgroundColor = { 16, 25, 30, 255 },
        }) {
            for _ in 0 ..< 5 {
                if clay.UI()({
                    layout = {
                        sizing = {
                            width = clay.SizingGrow({}),
                            height = clay.SizingFixed(48),
                        },
                        childAlignment = {
                            x = .Center,
                            y = .Center
                        }
                    },
                    backgroundColor = { 26, 35, 40, 255 },
                }) {
                    clay.Text("Hello World!", clay.TextConfig({ fontSize = 32 }))
                }
            }
        }

        cmds := clay.EndLayout()

        {
            resource: d3d11.MAPPED_SUBRESOURCE

            hr = device_context->Map(instance_buf, 0, .WRITE_DISCARD, {}, &resource)
            if (win32.FAILED(hr)) {
                fmt.eprintln("d3d11::IDeviceContext::Map failed:", hr)
                return
            }
            defer device_context->Unmap(instance_buf, 0)

            instances := cast([^]Instance)resource.pData

            for i in 0 ..< cmds.length {
                cmd := clay.RenderCommandArray_Get(&cmds, i)
                #partial switch cmd.commandType {
                case .Rectangle:
                    col := rgba(
                        u8(cmd.renderData.rectangle.backgroundColor.r),
                        u8(cmd.renderData.rectangle.backgroundColor.g),
                        u8(cmd.renderData.rectangle.backgroundColor.b),
                        u8(cmd.renderData.rectangle.backgroundColor.a),
                    )

                    radius := [4]f32{
                        cmd.renderData.rectangle.cornerRadius.topLeft,
                        cmd.renderData.rectangle.cornerRadius.topRight,
                        cmd.renderData.rectangle.cornerRadius.bottomRight,
                        cmd.renderData.rectangle.cornerRadius.bottomLeft,
                    }

                    instances[i] = { { cmd.boundingBox.x, cmd.boundingBox.y }, col, radius, { cmd.boundingBox.width, cmd.boundingBox.height } }
                case .Text:
                    text := cmd.renderData.text.stringContents
                    for rune in string(text.chars[:text.length]) {
                    }
                }
            }
        }

        device_context->ClearRenderTargetView(rtv, &[4]f32{0.25, 0.5, 1.0, 1.0})

        vertex_offset := u32(0)
        vertex_stride := u32(size_of(Vertex))

        instance_offset := u32(0)
        instance_stride := u32(size_of(Instance))

        device_context->IASetVertexBuffers(0, 1, &vertex_buf,   &vertex_stride,   &vertex_offset)
        device_context->IASetVertexBuffers(1, 1, &instance_buf, &instance_stride, &instance_offset)

        device_context->DrawInstanced(6, u32(cmds.length), 0, 0)

        hr = swap_chain->Present(1, {})
        if win32.FAILED(hr) {
            fmt.eprintln("d3d11::ISwapChain::Present failed:", hr)
            return;
        }
    }


    file, ok := os.create("atlas.pgm")
    defer os.close(file)

    os.write(file, transmute([]u8)(fmt.tprintf("P5\n%d %d\n255\n", 512, 512)))
    os.write(file, atlas.pixels)
}

rgba :: proc(r, g, b, a: u8) -> u32 {
    return (u32(a) << 24) |
           (u32(b) << 16) |
           (u32(g) << 8)  |
           (u32(r) << 0)
}

Contants :: struct #align(16) {
    mvp: matrix[4,4]f32
}

Vertex :: struct #align(16) {
    pos: [2]f32,
    uv:  [2]f32,
}

Instance :: struct #align(16) {
    off: [2]f32,
    col: u32,
    rad: [4]f32,
    dim: [2]f32,
}

handle_clay_errors :: proc "c" (error: clay.ErrorData) {
    context = runtime.default_context()
    fmt.eprintln(error)
}

measure_clay_text :: proc "c" (text: clay.StringSlice, config: ^clay.TextElementConfig, user_data: rawptr) -> clay.Dimensions {
    oc.set_size(&face, i32(config.fontSize) << 6, 72)

    context = runtime.default_context()

    width : oc.i26p6 = 0

    for rune in string(text.chars[:text.length]) {
        key := Font_Key{
            charcode = rune,
            font_size = oc.i26p6(config.fontSize) << 6,
        }

        metrics: oc.glyph_metrics

        if glyph, ok := glyphs[key]; ok {
            metrics = glyph.metrics
        } else {
            idx := oc.get_char_index(&face, u32(rune))
            oc.get_glyph_metrics(&face, idx, 0, &metrics)

            ext: oc.extent
            oc.render_glyph(&face, idx, &ext, nil, 0)

            rec := stbrp.Rect{
                w = stbrp.Coord(ext.cols),
                h = stbrp.Coord(ext.rows),
            }
            stbrp.pack_rects(&atlas.ctx, &rec, 1)

            oc.render_glyph(&face, idx, &ext, raw_data(atlas.pixels[rec.y * stbrp.Coord(atlas.width) + rec.x:]), atlas.width)
            
            fmt.println("rendering '", rune, "'")
            glyphs[key] = {
                x = rec.x, y = rec.y,
                w = rec.w, h = rec.h,
                metrics = metrics,
            }
        }

        width += metrics.advance + metrics.bearing_x;
    }

    return {
        width = f32(width >> 6),
        height = f32(oc.mul_16p16(oc.i16p16(face.descent + face.ascent), face.size.scale) >> 6),
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
    float2 uv  : TEX;

    float2 off : OFF;
    float4 col : COL;
    float4 rad : RAD;
    float2 dim : DIM;
};

struct vs_out {
    float4 pos : SV_POSITION;
    float2 uv  : TEX;
    float4 col : COL;
    float4 rad : RAD;
    float2 dim : DIM;
};

const float fade = 0.006;

vs_out vs_main(vs_in input) {
    vs_out output;
    float2 worldPos = input.off + input.pos * input.dim;
    output.pos = mul(mvp, float4(worldPos, 0.0, 1.0));
    output.uv = input.uv;
    output.col = input.col;
    output.rad = input.rad;
    output.dim = input.dim;
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
    uv.x *= input.dim.x / input.dim.y;

    float2 size = float2(input.dim.x / input.dim.y, 1.0) - (fade * 0.5);
    float4 radius = input.rad;

    float4 radii = min(radius.zywx, min(size.x, size.y));

    float d = sd_rounded_box(uv, size, radii);
    float a = 1.0 - smoothstep(0.0, fade, d);

    float4 col = input.col;
    col.a *= a;

    return col;
}
`
