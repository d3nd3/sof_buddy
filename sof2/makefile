BUILD ?= release
ODIR = obj/$(BUILD)
BDIR = bin
CC := i686-w64-mingw32-g++-posix
CFLAGS = -D_WIN32_WINNT=0x0501 -std=c++14 -O2 -Wall -Ihdr -Isrc -Isrc/engine
LFLAGS = -shared -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup
LIBS = -lws2_32 -lpsapi
OUT = $(BDIR)/sof2bud.dll
DEF = rsrc/sof2bud.def
SRC = $(wildcard src/*.cpp src/core/*.cpp src/engine/*.cpp features/*/*.cpp)

OBJS = $(patsubst %.cpp,$(ODIR)/%.o,$(SRC))

all: $(OUT)

$(ODIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJS) $(DEF)
	@mkdir -p $(BDIR)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(DEF) $(LIBS)

clean:
	rm -rf obj bin

.PHONY: all clean
