include ../common.mak

INCLUDEPATHS = -I../b_overdrive/ -I../b_whirl  -I../b_reverb -I../b_conv

override CFLAGS += $(INCLUDEPATHS)
override CFLAGS += -DSHAREDIR="\"$(sharedir)\""
LOADLIBES = -lm -lpthread
ifeq ($(XWIN),)
LOADLIBES += -ldl
endif

ifneq ($(DEBUG_TONEGEN_OSC),)
override CFLAGS += -DDEBUG_TONEGEN_OSC
endif

BXCC=$(CC)

OBJS= \
	midi.o \
	midi_aseq.o \
	state.o \
	vibrato.o \
	tonegen.o \
	program.o \
	pgmParser.o \
	cfgParser.o \
	main.o \
	midnam.o \
	memstream.o \
	libMTSClient.o \
	tuning.o \
	../b_reverb/reverb.cpp \
	../b_whirl/eqcomp.cpp \
	../b_whirl/whirl.cpp \
	../b_overdrive/overdrive.cpp

HEADERS = \
	cfgParser.h \
	global_inst.h \
	main.h \
	midi.h \
	midi_aseq.h \
	midi_types.h \
	pgmParser.h \
	program.h \
	vibrato.h \
	tonegen.h \
	defaultpgm.h \
	memstream.h \
	tuning.h \
	../b_reverb/reverb.h \
	../b_whirl/eqcomp.h \
	../b_whirl/whirl.h \
	../b_overdrive/overdrive.h


ifeq ($(shell $(PKG_CONFIG) --exists jack || echo no), no)
  $(error "JACK is required - install libjack-dev or libjack-jackd2-dev")
endif

ifeq ($(shell $(PKG_CONFIG) --exists alsa && echo yes)$(ENABLE_ALSA), yesyes)
  override CFLAGS+=-DHAVE_ASEQ `$(PKG_CONFIG) --cflags alsa`
  LOADLIBES+=`$(PKG_CONFIG) --libs alsa`
endif


override CFLAGS+=`$(PKG_CONFIG) --cflags jack`

ifeq ($(USEWEAKJACK),1)
  override CFLAGS+=-DUSE_WEAK_JACK -I$(RW)
  LOADLIBES+=-ldl
  WEAKJACK=$(RW)weakjack/weak_libjack.c
else
  LOADLIBES+=`$(PKG_CONFIG) --libs jack`
endif

ifeq ($(shell $(PKG_CONFIG) --exists sndfile \
	&& test -f /usr/include/zita-convolver.h -o -f /usr/local/include/zita-convolver.h \
	&& echo yes), $(ENABLE_CONVOLUTION))
  override CFLAGS+= -DHAVE_ZITACONVOLVE `$(PKG_CONFIG) --cflags sndfile`
  LOADLIBES+=-lzita-convolver `$(PKG_CONFIG) --libs sndfile`
  OBJS+=../b_conv/convolution.o
  HEADERS+=../b_conv/convolution.h
  BXCC=$(CXX)
endif

BUILT_EXE_PATH=../build/tuneBfree$(EXE_EXT)

all: setBfree$(EXE_EXT)
	rm -f $(BUILT_EXE_PATH)
	mkdir -p `dirname $(BUILT_EXE_PATH)`
	cp setBfree$(EXE_EXT) $(BUILT_EXE_PATH)

setBfree$(EXE_EXT): $(HEADERS) $(OBJS)
	echo foo $(CFLAGS)
	$(BXCC) $(CPPFLAGS) $(CFLAGS) \
		-o $(@) $(OBJS) $(WEAKJACK) \
		$(LDFLAGS) $(LOADLIBES)

programd: program.cpp pgmParser.cpp program.h pgmParser.h main.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wall \
		-DPRG_MAIN \
		-o $(@) program.cpp pgmParser.cpp

configd: cfgParser.cpp main.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wall \
		-DCFG_MAIN \
		-o $(@) cfgParser.cpp

program: programd ../pgm/default.pgm
	$(info re-generating default program include-file)
	./programd ../pgm/default.pgm > defaultpgm.h

program.o: defaultpgm.h

# override .o in parent dirs -- needed for parallel builds

../b_reverb/reverb.o: ../b_reverb/reverb.[ch]
	+$(MAKE) -C ../b_reverb reverb.o

../b_whirl/eqcomp.o: ../b_whirl/eqcomp.[ch]
	+$(MAKE) -C ../b_whirl eqcomp.o

../b_whirl/whirl.o: ../b_whirl/whirl.[ch]
	+$(MAKE) -C ../b_whirl whirl.o

../b_overdrive/overdrive.o: \
  ../b_overdrive/filterTools.[ch] \
  ../b_overdrive/overdrive.h \
  ../b_overdrive/overmaker.[ch] \
  ../b_overdrive/overmakerdefs.h \
  ../b_overdrive/ovt_biased.[ch]
	+$(MAKE) -C ../b_overdrive overdrive.o

../b_conv/convolution.o: ../b_conv/convolution.cc ../b_conv/convolution.h
	+$(MAKE) -C ../b_conv convolution.o

libMTSClient.o: ../libs/MTS-ESP/Client/libMTSClient.cpp
	$(CXX) -c ../libs/MTS-ESP/Client/libMTSClient.cpp -o libMTSClient.o

clean:
	rm -f *.o core *.log *~ setBfree$(EXE_EXT) configd programd tests

install: setBfree
	install -d $(DESTDIR)$(bindir)
	install -m755 setBfree $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(sharedir)/cfg
	install -d $(DESTDIR)$(sharedir)/pgm
	install -m644 ../cfg/*.cfg $(DESTDIR)$(sharedir)/cfg
	install -m644 ../pgm/*.pgm $(DESTDIR)$(sharedir)/pgm

uninstall:
	rm -f $(DESTDIR)$(bindir)/setBfree
	rm -f $(DESTDIR)$(sharedir)/cfg/*
	rm -f $(DESTDIR)$(sharedir)/pgm/*
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(sharedir)/cfg
	-rmdir $(DESTDIR)$(sharedir)/pgm
	-rmdir $(DESTDIR)$(sharedir)

tests: tuning.h tuning.cpp tonegen.h tonegen.cpp midi.h midi.cpp
	clang++ \
	-g \
	-Wall \
	-fsanitize=address,undefined \
	-o tests \
	-DTESTS \
	-I../libs/doctest/doctest \
	-I../libs/MTS-ESP/Client \
	../libs/MTS-ESP/Client/libMTSClient.cpp \
	tonegen.cpp \
	tuning.cpp \
	midi.cpp \

	./tests

.PHONY: clean all install uninstall program
