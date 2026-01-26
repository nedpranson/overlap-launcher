SOURCE := main.c

run:$(SOURCE)
	@mkdir bin
	zig build-exe --name overlap $(SOURCE) -lc -target x86_64-windows-gnu --subsystem windows -femit-bin=bin/overlap.exe

clean:
	@rm -rf bin
