package main

import "core:fmt"
import win32 "core:sys/windows"

main :: proc() {
    win32.MessageBoxW(
        nil,
        "Hello from Odin!",
        "Odin Win32",
        win32.MB_OK | win32.MB_ICONINFORMATION,
    )

    fmt.println("Hello World!")
}
