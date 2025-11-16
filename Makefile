SOURCE := main.c

run:$(SOURCE)
	@mkdir -p bin
	zig cc -target x86_64-windows -Wall -Wextra -Wpedantic $(SOURCE) -o bin/overlap.exe

clean:
	@rm -f bin/overlap.exe
	@rm -f bin/overlap.pdb
