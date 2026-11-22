# 32-bit GUI updater (Windows XP+)
DEFAULT_MINGW_CC := i686-w64-mingw32-g++-posix
ifeq ($(findstring mingw,$(CC)),)
CC := $(DEFAULT_MINGW_CC)
endif
WINDRES ?= i686-w64-mingw32-windres
CFLAGS = -std=c++14 -O2 -s -D_WIN32_WINNT=0x0501 -DUNICODE -D_UNICODE -mwindows -municode -I../../hdr
LIBS = -static -lcomctl32 -lshlwapi
OUT = ../../bin/sof_buddy_update.exe
VERSION_FILE = ../../VERSION
GENERATE_WINDOWS_RESOURCES_PY = ../../tools/generate_windows_resources.py
MANIFEST = sof_buddy_update.manifest
ICON = sof_buddy.ico
RESOURCE_RC = ../../build/updater/version_info.rc
RESOURCE_OBJ = ../../build/updater/version_info.o

all: $(OUT)

$(RESOURCE_RC): $(VERSION_FILE) $(GENERATE_WINDOWS_RESOURCES_PY) $(MANIFEST) $(ICON)
	@mkdir -p $(dir $@)
	python3 $(GENERATE_WINDOWS_RESOURCES_PY) --kind exe --version-file $(VERSION_FILE) --output $@

$(RESOURCE_OBJ): $(RESOURCE_RC) $(MANIFEST) $(ICON)
	@mkdir -p $(dir $@)
	$(WINDRES) -i $(RESOURCE_RC) -o $(RESOURCE_OBJ)

$(OUT): sof_buddy_update.cpp $(RESOURCE_OBJ)
	@mkdir -p ../../bin
	$(CC) $(CFLAGS) sof_buddy_update.cpp $(RESOURCE_OBJ) -o $(OUT) $(LIBS)

clean:
	rm -f $(OUT) $(RESOURCE_OBJ) $(RESOURCE_RC)

.PHONY: all clean
