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
CC = i686-w64-mingw32-g++-posix

# Directories
ODIR = obj
SDIR = src

# Include directory
INC = -Ihdr

# Compiler flags (default warnings)
COMMON_CFLAGS = -D_WIN32_WINNT=0x0501 -std=c++14

# Release compiler flags
CFLAGS = $(COMMON_CFLAGS) 
# Debug compiler flags (add -g for debugging symbols)
DBG_CFLAGS = $(COMMON_CFLAGS)

#What -static does: This is a broad, powerful linker flag that acts like a "hammer." 
#It tells the linker: "For every library that follows, attempt to use the static version (.a archive) instead
# of the shared version (linking to a .dll)."
#The Problem: While this correctly forces -lpthread to link the static libpthread.a, it also affects every other
# library linked later on, including the system libraries (-lws2_32, -lwinmm, etc.).
# It essentially tries to pull the code from those libraries directly into your DLL instead of just making your DLL
# call the ws2_32.dll that already exists on the user's Windows system.
#Why this is bad:
#Bloat: Your DLL would become huge because it would contain copies of Windows system functions.
#Instability/Errors: You should always link dynamically against core Windows system DLLs. They are the operating system.
# Attempting to statically link them is incorrect and often fails or produces unstable results.
# Linker flags
OFLAGS = -static -pthread -shared -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup
#OFLAGS = -shared -static-libgcc -static-libstdc++ -Wl,-Bdynamic -Wl,--enable-stdcall-fixup

# Source files
_SOURCE_DIRS = DetourXS features
SOURCES = $(wildcard $(addsuffix /*.cpp,$(_SOURCE_DIRS)))
_OBJS = $(patsubst $(SDIR)/%.cpp,$(ODIR)/%.o,$(SOURCES))

# Object files
OBJS = $(ODIR)/DetourXS/ADE32.o \
	$(ODIR)/DetourXS/detourxs.o \
	$(ODIR)/detour_tracker.o \
	$(ODIR)/wsock_entry.o \
	$(ODIR)/core.o \
	$(ODIR)/ref_gl.o \
	$(ODIR)/sof_buddy.o \
	$(ODIR)/crc32.o \
	$(ODIR)/util.o \
	$(ODIR)/cvars.o \
	$(ODIR)/features/feature_flags.o \
	$(ODIR)/features/cinematic_freeze/hooks.o \
	$(ODIR)/features/ref_fixes/hooks.o \
	$(ODIR)/features/media_timers/hooks.o \
	$(ODIR)/features/media_timers/media_timers.o \
	$(ODIR)/features/ref_fixes/ref_fixes.o \
	$(ODIR)/features/scaled_font/scaled_font.o

# Linking rule

$(OUT): detours-prebuild $(OBJS)
	@mkdir -p $(dir $(OUT))
	$(CC) $(OFLAGS) rsrc/sof_buddy.def $(OBJS) -o $(OUT) -lws2_32 -lwinmm -lshlwapi -lpsapi

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
	rm -rf $(ODIR) $(OUT) rsrc/detours_prebuild.md rsrc/detours_prebuild.txt

# Prebuild: generate detours list/header
.PHONY: detours-prebuild detours
detours-prebuild:
	@echo "Running detours prebuild script..."
	python3 scripts/list_detours.py || true

detours:
	@echo "Running detours prebuild script (detours target)..."
	python3 scripts/list_detours.py || true
	@echo "TXT: rsrc/detours_prebuild.txt" && head -n 20 rsrc/detours_prebuild.txt | cat
	@echo && echo "MD: rsrc/detours_prebuild.md" && head -n 40 rsrc/detours_prebuild.md | cat

