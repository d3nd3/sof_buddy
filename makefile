# Cross-compilation makefile for SoF Buddy
# Target: 32-bit Windows DLL

# Directories
BUILD ?= release
SDIR = src
ODIR = obj/$(BUILD)
BDIR = bin
IDIR = hdr
BDIR_GEN = build/$(BUILD)
FEATURES_TXT = features/FEATURES.txt
FEATURE_CONFIG_H = $(BDIR_GEN)/feature_config.h
FEATURE_LIST_H = $(BDIR_GEN)/feature_list.inc
VERSION_FILE = VERSION
VERSION_H = hdr/version.h
DETOURS_YAML = detours.yaml
GENERATED_DETOURS_H = $(BDIR_GEN)/generated_detours.h
GENERATED_DETOURS_CPP = $(BDIR_GEN)/generated_detours.cpp
GENERATED_REGISTRATIONS_H = $(BDIR_GEN)/generated_registrations.h
GENERATE_HOOKS_PY = tools/generate_hooks.py
GENERATE_FEATURES_TXT_PY = tools/generate_features_txt.py
GENERATE_MENU_EMBED_PY = tools/generate_menu_embed.py
MENU_DATA_CPP = $(SDIR)/features/internal_menus/menu_data.cpp
MENU_LIBRARY_DIR = $(SDIR)/features/internal_menus/menu_library

# Host toolchain for native utilities (not cross-compiled)
HOSTCC = g++
HOSTINC = -Isrc
HOSTCFLAGS = -std=c++17 -O2

# Compiler settings
CC = i686-w64-mingw32-g++-posix
INC = -I$(IDIR) -I$(SDIR) -I$(BDIR_GEN)
COMMON_CFLAGS = -D_WIN32_WINNT=0x0501 -std=c++14
DEPFLAGS = -MMD -MP
LIBS = -lws2_32 -lwinmm -lshlwapi -lpsapi -ldbghelp -lwinhttp

# Detect number of CPU cores for parallel builds
JOBS := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
MAKEFLAGS += -j$(JOBS)

# Build configurations
ifeq ($(BUILD),debug)
    CFLAGS = $(COMMON_CFLAGS) -g -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__
    TARGET_SUFFIX = 
else ifeq ($(BUILD),debug-gdb)
    CFLAGS = $(COMMON_CFLAGS) -g -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__ -DGDB
    TARGET_SUFFIX = 
else ifeq ($(BUILD),debug-collect)
    CFLAGS = $(COMMON_CFLAGS) -g -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__ -DSOFBUDDY_ENABLE_CALLSITE_LOGGER
    TARGET_SUFFIX = 
else ifeq ($(BUILD),xp-debug)
    CFLAGS = $(COMMON_CFLAGS) -O0 -g -fno-omit-frame-pointer -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__ -DSOFBUDDY_XP_BUILD
    TARGET_SUFFIX = 
else ifeq ($(BUILD),xp)
    CFLAGS = $(COMMON_CFLAGS) -O0 -fno-omit-frame-pointer -DNDEBUG -DSOFBUDDY_XP_BUILD
    TARGET_SUFFIX = 
else ifeq ($(BUILD),release)
    # With -fno-omit-frame-pointer, the game will not crash when using optimization.
    CFLAGS = $(COMMON_CFLAGS) -O3 -fno-omit-frame-pointer -DNDEBUG
    TARGET_SUFFIX = 
else
    $(error Unsupported BUILD='$(BUILD)' (use one of: release, debug, debug-gdb, debug-collect, xp, xp-debug))
endif

# Optional UI_MENU feature toggle (enable with: make UI_MENU=1 ...)
ifeq ($(UI_MENU),1)
    CFLAGS += -DUI_MENU
endif

# Output
OUT = $(BDIR)/sof_buddy.dll
DEF_FILE = rsrc/sof_buddy.def

# Linker flags
LFLAGS = -static -pthread -shared -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup
ifeq ($(BUILD),debug)
    LFLAGS += -Wl,--export-all-symbols -Wl,--add-stdcall-alias
else ifeq ($(BUILD),debug-gdb)
    LFLAGS += -Wl,--export-all-symbols -Wl,--add-stdcall-alias
else ifeq ($(BUILD),debug-collect)
    LFLAGS += -Wl,--export-all-symbols -Wl,--add-stdcall-alias
else ifeq ($(BUILD),xp-debug)
    LFLAGS += -Wl,--export-all-symbols -Wl,--add-stdcall-alias
endif

# Find all source files (no artificial limit)
SOURCES = $(shell find $(SDIR) -name "*.cpp")
SOURCES += $(MENU_DATA_CPP)
SOURCES := $(sort $(SOURCES))
OBJECTS = $(SOURCES:$(SDIR)/%.cpp=$(ODIR)/%.o)
DEPS = $(OBJECTS:.o=.d) $(ODIR)/generated_detours.d

# Find all hooks.json files
HOOKS_JSON = $(shell find $(SDIR)/features $(SDIR)/core -name "hooks.json" 2>/dev/null || true)
CALLBACKS_JSON = $(shell find $(SDIR)/features $(SDIR)/core -name "callbacks.json" 2>/dev/null || true)

# Default target
all: $(MENU_DATA_CPP) $(FEATURES_TXT) $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H) $(GENERATED_DETOURS_H) $(GENERATED_DETOURS_CPP) $(GENERATED_REGISTRATIONS_H) $(OUT)

FORCE:

SOFBUDDY_RMF_FILES = $(shell find $(MENU_LIBRARY_DIR) -name '*.rmf' 2>/dev/null || true)
$(MENU_DATA_CPP): $(GENERATE_MENU_EMBED_PY) $(SOFBUDDY_RMF_FILES)
	@python3 $(GENERATE_MENU_EMBED_PY)

# Create build directory
$(BDIR_GEN):
	@mkdir -p $(BDIR_GEN)

# Auto-generate FEATURES.txt from feature directories
$(FEATURES_TXT): $(GENERATE_FEATURES_TXT_PY)
	@echo "Auto-generating $(FEATURES_TXT) from feature directories..."
	@python3 $(GENERATE_FEATURES_TXT_PY)

# Generate feature configuration header from FEATURES.txt
$(FEATURE_CONFIG_H): $(FEATURES_TXT) | $(BDIR_GEN)
	@echo "Generating feature configuration from $(FEATURES_TXT)..."
	@echo "Active features:"
	@awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print " - " line }' $(FEATURES_TXT) || true
	@echo "Active feature count: $$(awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print line }' $(FEATURES_TXT) | wc -l)"
	@echo '#pragma once' > $@
	@echo '' >> $@
	@echo '/*' >> $@
	@echo '    Compile-time Feature Configuration' >> $@
	@echo '    ' >> $@
	@echo '    Auto-generated from $(FEATURES_TXT)' >> $@
	@echo '    Enable/disable features by commenting/uncommenting lines in $(FEATURES_TXT)' >> $@
	@echo '    ' >> $@
	@echo '    Format in $(FEATURES_TXT):' >> $@
	@echo '    - feature_name     (enabled)' >> $@
	@echo '    - // feature_name  (disabled)' >> $@
	@echo '*/' >> $@
	@echo '' >> $@
	@awk 'function strip(x){sub(/[ \t]#.*$$/,"",x); sub(/[ \t]+\/\/.*$$/,"",x); gsub(/^[ \t]+|[ \t]+$$/,"",x); return x} { line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,2)=="//") { feature=strip(substr(line,3)); if(feature!="") { macro="FEATURE_" feature; gsub(/-/,"_",macro); macro=toupper(macro); print "#define " macro " 0  // disabled" } next } if(substr(line,1,1)=="#") { feature=strip(substr(line,2)); if(feature!="" && feature!~ /^[A-Z]/) { macro="FEATURE_" feature; gsub(/-/,"_",macro); macro=toupper(macro); print "#define " macro " 0  // disabled" } next } feature=strip(line); macro="FEATURE_" feature; gsub(/-/,"_",macro); macro=toupper(macro); print "#define " macro " 1  // enabled" }' $(FEATURES_TXT) >> $@
	@echo '' >> $@
	@echo '/*' >> $@
	@echo '    Usage in feature files:' >> $@
	@echo '    ' >> $@
	@echo '    #include "feature_config.h"' >> $@
	@echo '    ' >> $@
	@echo '    #if FEATURE_MY_FEATURE' >> $@
	@echo '    REGISTER_HOOK(MyFunction, 0x12345678, void, __cdecl);' >> $@
	@echo '    // ... feature implementation' >> $@
	@echo '    #endif' >> $@
	@echo '*/' >> $@
	@echo "Generated feature configuration with $$(grep -c '^#define' $@) features"

# Generate feature list helper from FEATURES.txt
$(FEATURE_LIST_H): $(FEATURES_TXT) | $(BDIR_GEN)
	@echo "Generating feature list helper from $(FEATURES_TXT)..."
	@echo '/*' > $@
	@echo '    Auto-generated feature list helper' >> $@
	@echo '    Used by simple_init.cpp to display all features' >> $@
	@echo '    DO NOT EDIT - Generated from $(FEATURES_TXT)' >> $@
	@echo '*/' >> $@
	@echo '' >> $@
	@echo '#define FEATURE_LIST(macro)' >> $@
	@grep -v '^#' $(FEATURES_TXT) | grep -v '^$$' | while read line; do \
		if echo "$$line" | grep -q '^//'; then \
			feature=$$(echo "$$line" | sed 's|^//[[:space:]]*||'); \
			macro=$$(echo "$$feature" | tr '[:lower:]' '[:upper:]' | tr '-' '_'); \
			echo "macro(FEATURE_$$macro, \"$$feature\", \"$$feature\")" >> $@; \
		else \
			feature=$$line; \
			macro=$$(echo "$$feature" | tr '[:lower:]' '[:upper:]' | tr '-' '_'); \
			echo "macro(FEATURE_$$macro, \"$$feature\", \"$$feature\")" >> $@; \
		fi; \
	done

# Generate version header from VERSION file
$(VERSION_H): $(VERSION_FILE)
	@echo "Generating version header from $(VERSION_FILE)..."
	@echo "Current version: $$(cat $(VERSION_FILE) | tr -d '\r\n')"
	@echo '#pragma once' > $@
	@echo '' >> $@
	@echo '/*' >> $@
	@echo '    SoF Buddy Version Information' >> $@
	@echo '    ' >> $@
	@echo '    Auto-generated from $(VERSION_FILE)' >> $@
	@echo '    Increment version using: ./increment_version.sh' >> $@
	@echo '*/' >> $@
	@echo '' >> $@
	@echo '#define SOFBUDDY_VERSION "'$$(cat $(VERSION_FILE) | tr -d '\r\n')'"' >> $@
	@echo "Generated version header with version: $$(cat $(VERSION_FILE) | tr -d '\r\n')"

# Generate hook headers from detours.yaml, FEATURES.txt, and hooks.json files
$(GENERATED_DETOURS_H): $(DETOURS_YAML) $(FEATURES_TXT) $(GENERATE_HOOKS_PY) $(HOOKS_JSON) $(CALLBACKS_JSON) | $(BDIR_GEN)
	@echo "Generating hook headers from $(DETOURS_YAML)..."
	@python3 $(GENERATE_HOOKS_PY)
	@cp -f build/generated_detours.h $(GENERATED_DETOURS_H)
	@cp -f build/generated_detours.cpp $(GENERATED_DETOURS_CPP)
	@cp -f build/generated_registrations.h $(GENERATED_REGISTRATIONS_H)

$(GENERATED_REGISTRATIONS_H): $(GENERATED_DETOURS_H)
$(GENERATED_DETOURS_CPP): $(GENERATED_DETOURS_H)

# Compile generated_detours.cpp
$(ODIR)/generated_detours.o: $(GENERATED_DETOURS_CPP) $(GENERATED_DETOURS_H)
	@mkdir -p $(ODIR)
	$(CC) -c $(INC) $(DEPFLAGS) -o $@ $< $(CFLAGS)

$(ODIR)/generated_detours.d: $(GENERATED_DETOURS_CPP) $(GENERATED_DETOURS_H)
	@mkdir -p $(ODIR)
	@$(CC) -MM $(INC) $(CFLAGS) -MP -MT $(ODIR)/generated_detours.o -MF $@ $(GENERATED_DETOURS_CPP)

# Convenience target to print active features without building
features: $(FEATURES_TXT)
	@echo "Active features:"
	@awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print " - " line }' $(FEATURES_TXT) || true
	@echo "Active feature count: $$(awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print line }' $(FEATURES_TXT) | wc -l)"

# Build target
$(OUT): $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(GENERATED_DETOURS_H) $(GENERATED_REGISTRATIONS_H) $(OBJECTS) $(ODIR)/generated_detours.o
	@mkdir -p $(BDIR)
	$(CC) $(LFLAGS) $(DEF_FILE) $(OBJECTS) $(ODIR)/generated_detours.o -o $(OUT) $(LIBS)

# Object file rules - all depend on feature config, feature list, version header, and generated hook headers
$(ODIR)/%.o: $(SDIR)/%.cpp $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H) $(GENERATED_DETOURS_H) $(GENERATED_REGISTRATIONS_H)
	@mkdir -p $(dir $@)
	$(CC) -c $(INC) $(DEPFLAGS) -o $@ $< $(CFLAGS)

$(ODIR)/%.d: $(SDIR)/%.cpp $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H) $(GENERATED_DETOURS_H) $(GENERATED_REGISTRATIONS_H)
	@mkdir -p $(dir $@)
	@$(CC) -MM $(INC) $(CFLAGS) -MP -MT $(ODIR)/$*.o -MF $@ $<

-include $(DEPS)

# Build configurations
debug:
	$(MAKE) BUILD=debug all
debug-gdb:
	$(MAKE) BUILD=debug-gdb all
debug-collect:
	$(MAKE) BUILD=debug-collect all
release:
	$(MAKE) BUILD=release all
xp:
	$(MAKE) BUILD=xp all
xp-debug:
	$(MAKE) BUILD=xp-debug all

# Clean
clean:
	rm -rf obj build $(OUT)

# Show configuration
config:
	@echo "Compiler: $(CC)"
	@echo "Include: $(INC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo "Sources found: $(words $(SOURCES))"
	@echo "Target: $(OUT)"

.PHONY: all debug debug-gdb debug-collect release xp xp-debug clean config features compdb symbols FORCE

# ------------------------------
# Tools (host-native utilities)
# ------------------------------

.PHONY: tools funcmap-gen

tools: funcmap-gen

# Build the PE function map generator (native host tool)
funcmap-gen: tools/pe_funcmap_gen

tools/pe_funcmap_gen: tools/pe_funcmap_gen.cpp src/tools/pe_utils.cpp src/tools/pe_utils.h
	$(HOSTCC) $(HOSTCFLAGS) $(HOSTINC) -o $@ tools/pe_funcmap_gen.cpp src/tools/pe_utils.cpp

compdb:
	python3 tools/gen_compile_commands.py "$(CC)" "$(INC)" "$(CFLAGS)" $(SOURCES) > compile_commands.json
