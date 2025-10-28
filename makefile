# Cross-compilation makefile for SoF Buddy
# Target: 32-bit Windows DLL

# Directories
SDIR = src
ODIR = obj
BDIR = bin
IDIR = hdr
FEATURES_TXT = features/FEATURES.txt
FEATURE_CONFIG_H = hdr/feature_config.h
FEATURE_LIST_H = hdr/feature_list.inc
VERSION_FILE = VERSION
VERSION_H = hdr/version.h

# Host toolchain for native utilities (not cross-compiled)
HOSTCC = g++
HOSTINC = -Isrc
HOSTCFLAGS = -std=c++17 -O2

# Compiler settings
CC = i686-w64-mingw32-g++-posix
INC = -I$(IDIR) -I$(SDIR)
COMMON_CFLAGS = -D_WIN32_WINNT=0x0501 -std=c++17
LIBS = -lws2_32 -lwinmm -lshlwapi -lpsapi -ldbghelp

# Build configurations
ifeq ($(BUILD),debug)
    CFLAGS = $(COMMON_CFLAGS) -g -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__
    TARGET_SUFFIX = 
else
    # We don't use optimizations as it crashes the game.
    CFLAGS = $(COMMON_CFLAGS) -O0 -DNDEBUG
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

# Default target
all: $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H) $(OUT)

# Generate feature configuration header from FEATURES.txt
$(FEATURE_CONFIG_H): $(FEATURES_TXT)
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
	@echo '    #include "../../../hdr/feature_config.h"' >> $@
	@echo '    ' >> $@
	@echo '    #if FEATURE_MY_FEATURE' >> $@
	@echo '    REGISTER_HOOK(MyFunction, 0x12345678, void, __cdecl);' >> $@
	@echo '    // ... feature implementation' >> $@
	@echo '    #endif' >> $@
	@echo '*/' >> $@
	@echo "Generated feature configuration with $$(grep -c '^#define' $@) features"

# Generate feature list helper from FEATURES.txt
$(FEATURE_LIST_H): $(FEATURES_TXT)
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

# Convenience target to print active features without building
features:
	@echo "Active features:"
	@awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print " - " line }' $(FEATURES_TXT) || true
	@echo "Active feature count: $$(awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print line }' $(FEATURES_TXT) | wc -l)"

# Build target
$(OUT): $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(OBJECTS)
	@mkdir -p $(BDIR)
	$(CC) $(LFLAGS) $(DEF_FILE) $(OBJECTS) -o $(OUT) $(LIBS)

# Object file rules - all depend on feature config, feature list, and version header
$(ODIR)/%.o: $(SDIR)/%.cpp $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H)
	@mkdir -p $(dir $@)
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

# Build configurations
debug: 
	$(MAKE) BUILD=debug all
release: 
	$(MAKE) BUILD=release all

# Clean
clean:
	rm -rf $(ODIR) $(OUT) $(FEATURE_CONFIG_H) $(FEATURE_LIST_H) $(VERSION_H)

# Show configuration
config:
	@echo "Compiler: $(CC)"
	@echo "Include: $(INC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo "Sources found: $(words $(SOURCES))"
	@echo "Target: $(OUT)"

.PHONY: all debug release clean config features

# ------------------------------
# Tools (host-native utilities)
# ------------------------------

.PHONY: tools funcmap-gen

tools: funcmap-gen

# Build the PE function map generator (native host tool)
funcmap-gen: tools/pe_funcmap_gen

tools/pe_funcmap_gen: tools/pe_funcmap_gen.cpp src/pe_utils.cpp src/pe_utils.h
	$(HOSTCC) $(HOSTCFLAGS) $(HOSTINC) -o $@ tools/pe_funcmap_gen.cpp src/pe_utils.cpp