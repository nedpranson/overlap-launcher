.PHONY: build
build:
	odin build . -build-mode:object -target:windows_amd64 -out:obj/launcher.obj
	zig cc -target native-windows -o bin/launcher.exe obj/* fltused.c libs/clay.lib libs/onecore.lib -lbcrypt -ld3d11 -ld3dcompiler_47 -ldwrite
