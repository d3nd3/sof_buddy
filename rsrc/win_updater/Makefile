# 32-bit GUI updater (Windows XP+)
CC ?= i686-w64-mingw32-g++-posix
CFLAGS = -std=c++14 -O2 -s -D_WIN32_WINNT=0x0501 -DUNICODE -D_UNICODE -mwindows -municode
LIBS = -lcomctl32 -lshlwapi -static-libgcc -static-libstdc++
OUT = ../../bin/sof_buddy_update.exe

all: $(OUT)

$(OUT): sof_buddy_update.cpp
	@mkdir -p ../../bin
	$(CC) $(CFLAGS) sof_buddy_update.cpp -o $(OUT) $(LIBS)

clean:
	rm -f $(OUT)

.PHONY: all clean
