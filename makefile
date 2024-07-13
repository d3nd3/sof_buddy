# $@ The filename representing the target.
# $% The filename element of an archive member specification.
# $< The filename of the first prerequisite.
# $? The names of all prerequisites that are newer than the target, separated by spaces.
# $^ The filenames of all the prerequisites, separated by spaces. This list has duplicate filenames removed since for most uses, such as # #compiling, copying, etc., duplicates are not wanted.
# $+ Similar to $^, this is the names of all the prerequisites separated by spaces, except that $+ includes duplicates. This variable was #created for specific situations such as arguments to linkers where duplicate values have meaning.
# $* The stem of the target filename. A stem is typically a filename without its suffix. Its use outside of pattern rules is discouraged.


# Output binary
OUT = bin/sof_buddy.dll

# Compiler
CC = i686-w64-mingw32-g++

# Directories
ODIR = obj
SDIR = src

# Include directory
INC = -Ihdr

# Compiler flags -w (removes warnings)
COMMON_CFLAGS = -Wno-write-strings -w -fpermissive -std=c++14

# Release compiler flags
CFLAGS = $(COMMON_CFLAGS) 
# Debug compiler flags (add -g for debugging symbols)
DBG_CFLAGS = $(COMMON_CFLAGS) -g

# Linker flags
OFLAGS = -static -lpthread -shared -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup

# Source files
_SOURCE_DIRS = DetourXS features
SOURCES = $(wildcard $(addsuffix /*.cpp,$(_SOURCE_DIRS)))
_OBJS = $(patsubst $(SDIR)/%.cpp,$(ODIR)/%.o,$(SOURCES))

# Object files
OBJS = $(ODIR)/DetourXS/ADE32.o \
	$(ODIR)/DetourXS/detourxs.o \
	$(ODIR)/wsock_entry.o \
	$(ODIR)/sof_buddy.o \
	$(ODIR)/crc32.o \
	$(ODIR)/util.o \
	$(ODIR)/features/media_timers.o \
	$(ODIR)/features/ref_fixes.o \
	$(ODIR)/features/scaled_font.o

# Linking rule
$(OUT): $(OBJS)
	@mkdir -p $(dir $(OUT))
	$(CC) $(OFLAGS) rsrc/sof_buddy.def $^ -o $(OUT) -lws2_32 -lwinmm -lshlwapi -lpsapi

# Debug target
debug: CFLAGS = $(DBG_CFLAGS)
debug: $(OUT)

# Build rule
$(ODIR)/%.o: $(SDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)


# Clean rule
.PHONY: clean
clean:
	rm -rf $(ODIR) $(OUT)

