package onecore

import "core:c"

foreign import onecore "onecore.lib"

i16p16 :: i32
i26p6  :: i32

library         :: distinct struct {}
face_impl       :: distinct struct {}
collection_impl :: distinct struct {}

error :: enum c.int {
    ok,
    invalid_param,
    table_missing,
    out_of_memory,
    failed_to_open,
    invalid_pixel_size,
    unexpected,
}

open_params :: struct {
    face_index: u32,
    desired_size: i26p6,
    dpi: u16,
}

face :: struct {
    impl: ^face_impl,

    size: size,
    upem: u16,
    ascent: u16,
    descent: u16,
    leading: i16,
    underline_position: i16,
    underline_thickness: u16,
};

glyph_metrics :: struct {
    width: i26p6,
    height: i26p6,
    bearing_x: i26p6,
    bearing_y: i26p6,
    advance: i26p6,
}

size :: struct {
    ppem: u16,
    scale: i16p16,
}

slant :: enum c.int {
    roman,
    italic,
    oblique,
}

font :: struct {
    family: cstring,
    slant: slant,
    weight: u16,
}

collection :: struct {
    impl: ^collection_impl,

    fonts: [^]^font,
    nfonts: u32,
}

extent :: struct {
    rows: u32,
    cols: u32,
}

@(link_prefix = "oc_", default_calling_convention = "c")
foreign onecore {
    init_library :: proc(olibrary: ^^library) -> error ---
    free_library :: proc(library: ^library) ---
    mul_16p16    :: proc(a: i26p6, b: i26p6) -> i26p6 ---
    div_16p16    :: proc(a: i26p6, b: i26p6) -> i26p6 ---
}

@(link_prefix = "ocl_", default_calling_convention = "c")
foreign onecore {
    open_face          :: proc(library: ^library, path: cstring, uparams: ^open_params, oface: ^face) -> error ---
    free_face          :: proc(face: ^face) ---
    set_size           :: proc(face: ^face, desired_size: i26p6, dpi: u16) -> error ---
    get_char_index     :: proc(face: ^face, charcode: u32) -> u16 ---
    get_glyph_metrics  :: proc(face: ^face, index: u16, flags: u32, ometrics: ^glyph_metrics) ---
    render_glyph       :: proc(face: ^face, index: u16, oextent: ^extent, buffer: rawptr, pitch: c.size_t) -> error ---
}

@(link_prefix = "ocf_", default_calling_convention = "c")
foreign onecore {
    init_collection :: proc(library: ^library, ocollection: ^collection) -> error ---
    free_collection :: proc(collection: ^collection) ---
    load_fonts      :: proc(collection: ^collection) -> error ---
}
