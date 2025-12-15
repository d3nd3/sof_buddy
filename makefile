# Cross-compilation makefile for SoF Buddy
# Target: 32-bit Windows DLL

# Directories
SDIR = src
ODIR = obj
BDIR = bin
IDIR = hdr
BDIR_GEN = build
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

# Host toolchain for native utilities (not cross-compiled)
HOSTCC = g++
HOSTINC = -Isrc
HOSTCFLAGS = -std=c++17 -O2

# Compiler settings
CC = i686-w64-mingw32-g++-posix
INC = -I$(IDIR) -I$(SDIR) -I$(BDIR_GEN)
COMMON_CFLAGS = -D_WIN32_WINNT=0x0501 -std=c++14
LIBS = -lws2_32 -lwinmm -lshlwapi -lpsapi -ldbghelp

# Detect number of CPU cores for parallel builds
JOBS := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
MAKEFLAGS += -j$(JOBS)

# Build configurations
ifeq ($(BUILD),debug)
    CFLAGS = $(COMMON_CFLAGS) -g -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__
    TARGET_SUFFIX = 
else ifeq ($(BUILD),xp-debug)
    CFLAGS = $(COMMON_CFLAGS) -O0 -g -fno-omit-frame-pointer -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__
    TARGET_SUFFIX = 
else ifeq ($(BUILD),xp)
    CFLAGS = $(COMMON_CFLAGS) -O0 -fno-omit-frame-pointer -DNDEBUG
    TARGET_SUFFIX = 
else
    # With -fno-omit-frame-pointer, the game will not crash when using optimization.
    CFLAGS = $(COMMON_CFLAGS) -O3 -fno-omit-frame-pointer -DNDEBUG
    TARGET_SUFFIX = 
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

# Find all source files (no artificial limit)
SOURCES = $(shell find $(SDIR) -name "*.cpp")
OBJECTS = $(SOURCES:$(SDIR)/%.cpp=$(ODIR)/%.o)

# Find all hooks.json files
HOOKS_JSON = $(shell find $(SDIR)/features $(SDIR)/core -name "hooks.json" 2>/dev/null || true)

# Default target
all: $(FEATURES_TXT) $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H) $(GENERATED_DETOURS_H) $(GENERATED_DETOURS_CPP) $(GENERATED_REGISTRATIONS_H) $(OUT)

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
	@grep -v '^#' $(FEATURES_TXT) | grep -v '^$$' | while read line; do \
		if echo "$$line" | grep -q '^//'; then \
			feature=$$(echo "$$line" | sed 's|^//[[:space:]]*||'); \
			macro=$$(echo "FEATURE_$$feature" | tr '[:lower:]' '[:upper:]' | tr '-' '_'); \
			echo "#define $$macro 0  // disabled" >> $@; \
		else \
			macro=$$(echo "FEATURE_$$line" | tr '[:lower:]' '[:upper:]' | tr '-' '_'); \
			echo "#define $$macro 1  // enabled" >> $@; \
		fi; \
	done
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

# Generate hook headers from detours.yaml and hooks.json files
$(GENERATED_DETOURS_H): $(DETOURS_YAML) $(GENERATE_HOOKS_PY) $(HOOKS_JSON) | $(BDIR_GEN)
	@echo "Generating hook headers from $(DETOURS_YAML)..."
	@python3 $(GENERATE_HOOKS_PY)

$(GENERATED_REGISTRATIONS_H): $(GENERATED_DETOURS_H)
$(GENERATED_DETOURS_CPP): $(GENERATED_DETOURS_H)

# Compile generated_detours.cpp
$(ODIR)/generated_detours.o: $(GENERATED_DETOURS_CPP) $(GENERATED_DETOURS_H)
	@mkdir -p $(ODIR)
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

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
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

# Build configurations
debug: 
	$(MAKE) BUILD=debug all
release: 
	$(MAKE) BUILD=release all
xp:
	$(MAKE) BUILD=xp all
xp-debug:
	$(MAKE) BUILD=xp-debug all

# Clean
clean:
	rm -rf $(ODIR) $(OUT) $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H) $(BDIR_GEN)

# Show configuration
config:
	@echo "Compiler: $(CC)"
	@echo "Include: $(INC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo "Sources found: $(words $(SOURCES))"
	@echo "Target: $(OUT)"

.PHONY: all debug release xp xp-debug clean config features compdb

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