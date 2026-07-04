.PHONY: build
build:
	odin build . -build-mode:object -target:windows_amd64 -out:obj/launcher.obj
	zig cc -target native-windows -Os -o bin/launcher.exe obj/* fltused.c libs/clay.lib -lbcrypt -ld3d11 -ld3dcompiler_47
