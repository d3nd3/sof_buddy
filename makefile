# Cross-compilation makefile for SoF Buddy
# Target: 32-bit Windows DLL

# Directories
SDIR = src
ODIR = obj
BDIR = bin
IDIR = hdr
FEATURES_TXT = features/FEATURES.txt
FEATURE_CONFIG_H = hdr/feature_config.h

# Compiler settings
CC = i686-w64-mingw32-g++-posix
INC = -I$(IDIR) -I$(SDIR)
COMMON_CFLAGS = -D_WIN32_WINNT=0x0501 -std=c++17
LIBS = -lws2_32 -lwinmm -lshlwapi -lpsapi -ldbghelp

# Build configurations
ifeq ($(BUILD),release)
    CFLAGS = $(COMMON_CFLAGS) -O2 -DNDEBUG
    TARGET_SUFFIX = 
else
    CFLAGS = $(COMMON_CFLAGS) -g -D__LOGGING__ -D __FILEOUT__ -D __TERMINALOUT__
    TARGET_SUFFIX = 
endif

# Output
OUT = $(BDIR)/sof_buddy.dll
DEF_FILE = rsrc/sof_buddy.def

# Linker flags
LFLAGS = -static -pthread -shared -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup

# Find all source files
SOURCES = $(shell find $(SDIR) -name "*.cpp" | head -50)
OBJECTS = $(SOURCES:$(SDIR)/%.cpp=$(ODIR)/%.o)

# Default target
all: $(FEATURE_CONFIG_H) $(OUT)

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

# Convenience target to print active features without building
features:
	@echo "Active features:"
	@awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print " - " line }' $(FEATURES_TXT) || true
	@echo "Active feature count: $$(awk '{ line=$$0; sub(/\r$$/,"",line); gsub(/^[ \t]+|[ \t]+$$/,"",line); if(line=="") next; if(substr(line,1,1)=="#") next; if(substr(line,1,2)=="//") next; print line }' $(FEATURES_TXT) | wc -l)"

# Build target
$(OUT): $(FEATURE_CONFIG_H) $(OBJECTS)
	@mkdir -p $(BDIR)
	$(CC) $(LFLAGS) $(DEF_FILE) $(OBJECTS) -o $(OUT) $(LIBS)

# Object file rules - all depend on feature config
$(ODIR)/%.o: $(SDIR)/%.cpp $(FEATURE_CONFIG_H)
	@mkdir -p $(dir $@)
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

# Build configurations
debug: all
release: 
	$(MAKE) BUILD=release all

# Clean
clean:
	rm -rf $(ODIR) $(OUT) $(FEATURE_CONFIG_H)

# Show configuration
config:
	@echo "Compiler: $(CC)"
	@echo "Include: $(INC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo "Sources found: $(words $(SOURCES))"
	@echo "Target: $(OUT)"

.PHONY: all debug release clean config features