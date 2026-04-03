WATCOM_ROOT ?= vendor/openwatcom-v2/current-build-2026-04-03
WATCOM_BIN ?= $(WATCOM_ROOT)/binl64
WCC = $(WATCOM_BIN)/wcc
WLINK = $(WATCOM_BIN)/wlink
WASM = $(WATCOM_BIN)/wasm
PYTHON = python3

EXTRA_CFLAGS ?=
CFLAGS = -i=include -ms -s -zq -bt=dos $(EXTRA_CFLAGS)
LDFLAGS = system dos option quiet

OBJS = \
    build/main.obj \
    build/cfg.obj \
    build/ui.obj \
    build/launch.obj

FAKEGAME_OBJS = \
    build/fakegame.obj

all: build aml2.exe amlstub.com
test-build: build aml2.exe amlstub.com fakegame.exe

build:
	mkdir -p build

build/main.obj: src/main.c include/aml.h include/cfg.h include/ui.h include/launch.h
	$(WCC) $(CFLAGS) -fo=build/main.obj src/main.c

build/cfg.obj: src/cfg.c include/aml.h include/cfg.h
	$(WCC) $(CFLAGS) -fo=build/cfg.obj src/cfg.c

build/ui.obj: src/ui.c include/aml.h include/ui.h
	$(WCC) $(CFLAGS) -fo=build/ui.obj src/ui.c

build/launch.obj: src/launch.c include/aml.h include/launch.h
	$(WCC) $(CFLAGS) -fo=build/launch.obj src/launch.c

build/fakegame.obj: tests/fakegame.c
	$(WCC) $(CFLAGS) -fo=build/fakegame.obj tests/fakegame.c

build/amlstub.obj: stub/amlstub.asm
	$(WASM) -0 -bt=dos -mt -zq -zcm=tasm -fo=build/amlstub.obj -fr=build/amlstub.err stub/amlstub.asm

aml2.exe: $(OBJS)
	$(WLINK) $(LDFLAGS) name aml2.exe file { $(OBJS) }

amlstub.com: build/amlstub.obj tools/obj2com.py
	$(PYTHON) tools/obj2com.py build/amlstub.obj amlstub.com

fakegame.exe: $(FAKEGAME_OBJS)
	$(WLINK) $(LDFLAGS) name fakegame.exe file { $(FAKEGAME_OBJS) }

clean:
	rm -rf build aml2.exe aml2.com amlstub.exe amlstub.com fakegame.exe AML2.RUN AML2.AUT AML2.TRC out
