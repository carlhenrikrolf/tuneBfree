EXPORTED_VERSION=0.8.12
export EXPORTED_VERSION

FONTFILE ?= verabd.h
export FONTFILE

ROBTK ?= libs/robtk/

ifneq ($(ROBTK),)
RW=$(abspath $(ROBTK))/
export RW
endif

# Make LV2, CLI, and GUI
SUBDIRS = b_synth src ui

default: all

$(SUBDIRS)::
	$(MAKE) -C $@ $(MAKECMDGOALS)

all clean install uninstall: $(SUBDIRS)

doc:
	help2man -N --help-option=-H -n 'DSP tonewheel organ' -o doc/setBfree.1 src/setBfree

dist:
	git archive --format=tar --prefix=setbfree-$(EXPORTED_VERSION)/ HEAD | gzip -9 > setbfree-$(EXPORTED_VERSION).tar.gz

test:
	pytest -v tests

format:
	clang-format -i `find . -name '*.cpp' ! -path '*/libs/*'`

formatcheck:
	clang-format --dry-run --Werror `find . -name '*.cpp' ! -path '*/libs/*'`

.PHONY: clean all subdirs install uninstall dist doc
