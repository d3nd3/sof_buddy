# $@ The filename representing the target.
# $% The filename element of an archive member specification.
# $< The filename of the first prerequisite.
# $? The names of all prerequisites that are newer than the target, separated by spaces.
# $^ The filenames of all the prerequisites, separated by spaces. This list has duplicate filenames removed since for most uses, such as # #compiling, copying, etc., duplicates are not wanted.
# $+ Similar to $^, this is the names of all the prerequisites separated by spaces, except that $+ includes duplicates. This variable was #created for specific situations such as arguments to linkers where duplicate values have meaning.
# $* The stem of the target filename. A stem is typically a filename without its suffix. Its use outside of pattern rules is discouraged.


OUT = \
	bin/sof_buddy.dll
CC = \
	i686-w64-mingw32-g++
ODIR = \
	obj
SDIR = \
	src
INC = \
	-Ihdr
CFLAGS = \
	-Wno-write-strings -w -fpermissive -std=c++14
OFLAGS = \
	-static -lpthread -shared -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup
_OBJS = \
	DetourXS/ADE32.o \
	DetourXS/detourxs.o \
	wsock_entry.o \
	sof_buddy.o \
	crc32.o \
	util.o \
	features/media_timers.o \
	features/ref_fixes.o
OBJS = \
	$(patsubst %,$(ODIR)/%,$(_OBJS))

$(ODIR)/%.o: $(SDIR)/%.cpp
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

$(OUT): $(OBJS)
	$(CC) $(OFLAGS) rsrc/sof_buddy.def $^ -o$(OUT) -lws2_32 -lwinmm -lpathcch

.PHONY: clean
clean:
	rm -f $(ODIR)/*.o $(ODIR)/DetourXS/*.o $(ODIR)/features/*.o $(OUT)
