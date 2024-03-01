include ../common.mak
TX=../b_synth/textures/
WINDRES=windres

override CFLAGS+= -I../src -I../b_overdrive -I../b_reverb -I../b_whirl -I../b_synth/
override CFLAGS+=`$(PKG_CONFIG) --cflags lv2` -DLV2SYNTH -DREQUIRE_UI

ifeq ($(FONTFILE),verabd.h)
  override CFLAGS+= -DBUILTINFONT
endif

JACKEXTRA=
OSXJACKWRAP=

ifeq ($(IS_OSX), yes)
  OSXJACKWRAP=$(RW)jackwrap.mm
  USEWEAKJACK=1
  CC=$(CXX)
else
  ifeq ($(IS_WIN), yes)
    CC=$(CXX)
    UIDEPS+=win_icon.rc.o
    JACKEXTRA+=win_icon.rc.o
    USEWEAKJACK=1
    override CFLAGS+=-mwindows
  else
    LOADLIBES=-lrt
    UILIBS+=-ldl
  endif
endif

ifeq ($(USEWEAKJACK),1)
  UICFLAGS+=-DUSE_WEAK_JACK
  JACKEXTRA+=$(RW)weakjack/weak_libjack.cpp
else
  ifeq ($(shell $(PKG_CONFIG) --exists jack || echo no), no)
    $(error "JACK is required - install libjack-dev or libjack-jackd2-dev")
  endif
  UILIBS+=`$(PKG_CONFIG) --libs jack`
endif

ifeq ($(shell $(PKG_CONFIG) --exists liblo && echo yes), yes)
  UICFLAGS+=`$(PKG_CONFIG) $(PKG_UI_FLAGS) --cflags liblo` -DHAVE_LIBLO
  UILIBS+=`$(PKG_CONFIG) $(PKG_UI_FLAGS) --libs liblo`
endif

targets=

DESC=$(abspath desc.h)

ifeq ($(XWIN),)
  ifeq ($(LV2AVAIL)$(HAVE_UI)$(HAVE_IDLE), yesyesyes)
    targets+=setBfreeUI$(EXE_EXT)
  else
    $(warning standalone JACK/GL-GUI will not be built)
  endif
endif

BUILT_UI_EXE_PATH=../build/tuneBfreeUI$(EXE_EXT)

all: $(targets)
	rm -f $(BUILT_UI_EXE_PATH)
	mkdir -p `dirname $(BUILT_UI_EXE_PATH)`
	-cp setBfreeUI$(EXE_EXT) $(BUILT_UI_EXE_PATH)

setBfreeUI$(EXE_EXT): $(RW)jackwrap.cpp $(LV2OBJ) $(OSXJACKWRAP) $(RW)weakjack/weak_libjack.def $(RW)weakjack/weak_libjack.h $(JACKEXTRA) \
	../b_synth/lv2.cpp ../b_synth/uris.h desc.h \
	../b_synth/ui.cpp $(UIDEPS) \
	../src/cfgParser.h ../src/midi.h
	$(MAKE) -C ../b_synth $(FONTFILE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(UICFLAGS) $(LIC_CFLAGS) \
		-pthread \
		-DXTERNAL_UI -DJACK_AUTOCONNECT=15 \
		-DAPPNAME="\"setBfree\"" \
		-DJACK_DESCRIPT=\"$(DESC)\" \
		-o setBfreeUI$(EXE_EXT) \
		$(RW)jackwrap.cpp $(OSXJACKWRAP) $(JACKEXTRA) \
		$(LV2OBJ) ../b_synth/lv2.cpp ../b_synth/ui.cpp \
		$(LDFLAGS) $(UILIBS) $(LOADLIBES) $(LIC_LOADLIBES) \
		-lm

# windows icon
.SUFFFIXES: .rc
win_icon.rc.o: win_icon.rc ../img/setbfree.ico
	$(WINDRES) -o $@ $<

install: $(targets)
ifneq ($(targets),)
	install -d $(DESTDIR)$(bindir)
	install -m755 setBfreeUI$(EXE_EXT) $(DESTDIR)$(bindir)
endif

uninstall:
	rm -f $(DESTDIR)$(bindir)/setBfreeUI$(EXE_EXT)

clean:
	rm -f setBfreeUI$(EXE_EXT)
	rm -f win_icon.rc.o

.PHONY: clean all install uninstall
