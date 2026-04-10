WATCOM_ROOT ?= vendor/openwatcom-v2/current-build-2026-04-03
WATCOM_BIN ?= $(WATCOM_ROOT)/binl64
WCC = $(WATCOM_BIN)/wcc
WLINK = $(WATCOM_BIN)/wlink
WASM = $(WATCOM_BIN)/wasm
PYTHON = python3

EXTRA_CFLAGS ?=
CFLAGS = -i=include -ms -s -zq -bt=dos $(EXTRA_CFLAGS)
LDFLAGS = system dos option quiet

CORE_OBJS = \
	build/main.obj \
	build/cfg.obj \
	build/launch.obj

UI_OBJS = \
	build/ui_auto.obj \
	build/ui_bigtext.obj \
	build/ui_blit.obj \
	build/ui_edit.obj \
	build/ui_loop.obj \
	build/ui_core.obj \
	build/ui_state.obj \
	build/ui_test.obj

OBJS = \
	$(CORE_OBJS) \
	$(UI_OBJS)

FAKEGAME_OBJS = \
	build/fakegame.obj

VIDTEST_SRCS = \
	tests/video/test_001.c \
	tests/video/test_002.c \
	tests/video/test_003.c \
	tests/video/test_004.c \
	tests/video/test_005.c \
	tests/video/test_006.c

VIDTEST_EXES = \
	test_001.exe \
	test_002.exe \
	test_003.exe \
	test_004.exe \
	test_005.exe \
	test_006.exe

all: build amlui.exe aml.com $(VIDTEST_EXES)
test-build: build amlui.exe aml.com fakegame.exe $(VIDTEST_EXES)

build:
	mkdir -p build

include/aml_build.h:
	printf '#ifndef AML_BUILD_H\n#define AML_BUILD_H\n\n#define AML_BUILD_VERSION "%s"\n\n#endif\n' "$${AML_BUILD_VERSION:-local}" > include/aml_build.h

build/main.obj: src/main.c include/aml.h include/cfg.h include/ui.h include/launch.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/main.obj src/main.c

build/cfg.obj: src/cfg.c include/aml.h include/cfg.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/cfg.obj src/cfg.c

build/ui_auto.obj: src/ui_auto.c src/ui_int.h src/ui_ops.h include/aml.h include/ui.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/ui_auto.obj src/ui_auto.c

build/ui_edit.obj: src/ui_edit.c src/ui_int.h src/ui_ops.h include/aml.h include/ui.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/ui_edit.obj src/ui_edit.c

build/ui_bigtext.obj: src/ui_bigtext.c src/ui_int.h include/aml.h include/ui.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/ui_bigtext.obj src/ui_bigtext.c

build/ui_blit.obj: src/ui_blit.asm
	$(WASM) -0 -bt=dos -ms -zq -zcm=tasm -fo=build/ui_blit.obj -fr=build/ui_blit.err src/ui_blit.asm

build/ui_loop.obj: src/ui_loop.c src/ui_int.h src/ui_ops.h include/aml.h include/ui.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/ui_loop.obj src/ui_loop.c

build/ui_core.obj: src/ui_core.c src/ui_int.h include/aml.h include/ui.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/ui_core.obj src/ui_core.c

build/ui_state.obj: src/ui_state.c src/ui_int.h src/ui_state.h include/aml.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/ui_state.obj src/ui_state.c

build/ui_test.obj: src/ui_test.c src/ui_int.h include/aml.h include/ui.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/ui_test.obj src/ui_test.c

build/launch.obj: src/launch.c include/aml.h include/launch.h include/aml_build.h
	$(WCC) $(CFLAGS) -fo=build/launch.obj src/launch.c

build/fakegame.obj: tests/dos/fakegame.c
	$(WCC) $(CFLAGS) -fo=build/fakegame.obj tests/dos/fakegame.c

build/amlstub.obj: stub/amlstub.asm
	$(WASM) -0 -bt=dos -mt -zq -zcm=tasm -fo=build/amlstub.obj -fr=build/amlstub.err stub/amlstub.asm

amlui.exe: $(OBJS)
	$(WLINK) $(LDFLAGS) name amlui.exe file { $(OBJS) }

aml.com: build/amlstub.obj tools/obj2com.py
	$(PYTHON) tools/obj2com.py build/amlstub.obj aml.com

fakegame.exe: $(FAKEGAME_OBJS)
	$(WLINK) $(LDFLAGS) name fakegame.exe file { $(FAKEGAME_OBJS) }

build/test_001.obj: tests/video/test_001.c
	$(WCC) $(CFLAGS) -fo=build/test_001.obj tests/video/test_001.c

build/test_002.obj: tests/video/test_002.c tests/video/vidtest.h
	$(WCC) $(CFLAGS) -i=tests/video -fo=build/test_002.obj tests/video/test_002.c

build/test_003.obj: tests/video/test_003.c tests/video/vidtest.h
	$(WCC) $(CFLAGS) -i=tests/video -fo=build/test_003.obj tests/video/test_003.c

build/test_004.obj: tests/video/test_004.c tests/video/vidtest.h
	$(WCC) $(CFLAGS) -i=tests/video -fo=build/test_004.obj tests/video/test_004.c

build/test_005.obj: tests/video/test_005.c tests/video/vidtest.h
	$(WCC) $(CFLAGS) -i=tests/video -fo=build/test_005.obj tests/video/test_005.c

test_001.exe: build/test_001.obj
	$(WLINK) $(LDFLAGS) name test_001.exe file { build/test_001.obj }

test_002.exe: build/test_002.obj
	$(WLINK) $(LDFLAGS) name test_002.exe file { build/test_002.obj }

test_003.exe: build/test_003.obj
	$(WLINK) $(LDFLAGS) name test_003.exe file { build/test_003.obj }

test_004.exe: build/test_004.obj
	$(WLINK) $(LDFLAGS) name test_004.exe file { build/test_004.obj }

test_005.exe: build/test_005.obj
	$(WLINK) $(LDFLAGS) name test_005.exe file { build/test_005.obj }

build/test_006.obj: tests/video/test_006.c tests/video/vidtest.h
	$(WCC) $(CFLAGS) -i=tests/video -fo=build/test_006.obj tests/video/test_006.c

test_006.exe: build/test_006.obj
	$(WLINK) $(LDFLAGS) name test_006.exe file { build/test_006.obj }

clean:
	rm -rf build amlui.exe amlui.com aml.exe aml.com fakegame.exe $(VIDTEST_EXES) AML2.RUN AML2.AUT AML2.TRC out include/aml_build.h
