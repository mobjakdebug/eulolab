# Cross-build an OpenOrbis PS4 eboot.bin and PKG candidate (console-untested).
# Use the pinned OpenOrbis v0.5.4 release; do not put SDK binaries in Git.
ROOT := $(abspath ..)
SDK := $(OO_PS4_TOOLCHAIN)
ifeq ($(strip $(SDK)),)
$(error Set OO_PS4_TOOLCHAIN to the extracted OpenOrbis PS4 toolchain directory)
endif
CLANG ?= clang++-18
LLD ?= ld.lld-18
PKGTOOL := $(SDK)/bin/linux/PkgTool.Core
FSELF := $(SDK)/bin/linux/create-fself
GP4 := $(SDK)/bin/linux/create-gp4
OUT := $(ROOT)/out/ps4
STAGE := $(OUT)/stage
CONTENT_ID := IV0000-BREW00242_00-EULOROCKETLAB001
SRC := core/rocket.cpp core/motor.cpp core/flight.cpp core/telemetry.cpp core/fin_inspection.cpp core/mesh.cpp core/bench_profile.cpp core/bench_link.cpp ui/canvas.cpp ui/rocket_view.cpp ui/app.cpp ui/app_draw.cpp platform/ps4.cpp platform/ps4_bench_usb.cpp
OBJ := $(addprefix $(OUT)/obj/,$(SRC:.cpp=.o))
HEADERS := $(wildcard $(ROOT)/src/core/*.hpp $(ROOT)/src/ui/*.hpp) $(ROOT)/src/platform/ps4_bench_usb.hpp $(ROOT)/ps4/include/libusb.h
# The SDK's libc++ expects musl's BSD and XOPEN declarations (nanosleep and
# vasprintf); gnu++17 retains the feature macros that its own samples use.
FLAGS := --target=x86_64-pc-freebsd12-elf -std=gnu++17 \
  -O2 -fPIC -funwind-tables -isysroot $(SDK) -isystem $(SDK)/include \
  -isystem $(SDK)/include/c++/v1 -I$(ROOT)/ps4/include -I$(OUT) -I$(ROOT)/src
# No USB VID/PID or pin profile is compiled into the eboot. The console app
# edits a separate /data/eulolab/bench-profile.cfg; defaults open no device.
LIBS := -lc -lkernel -lc++ -lSceVideoOut -lScePad -lSceUserService -lSceSysmodule -lSceUsbd
.PHONY: eboot pkg clean

eboot: $(OUT)/eboot.bin

$(OUT)/obj/%.o: $(ROOT)/src/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CLANG) $(FLAGS) -c $< -o $@

$(OUT)/eulolab.elf: $(OBJ)
	$(LLD) -m elf_x86_64 -pie --script $(SDK)/link.x --eh-frame-hdr \
	  -L$(SDK)/lib $(OBJ) -o $@ $(LIBS) $(SDK)/lib/crt1.o

$(OUT)/eboot.bin: $(OUT)/eulolab.elf
	$(FSELF) -in=$< -out=$(OUT)/eulolab.oelf --eboot "$@" --paid 0x3800000000000011

$(STAGE)/eboot.bin: $(OUT)/eboot.bin
	@mkdir -p $(STAGE)
	cp $< $@

$(STAGE)/assets/font-atlas.bin: $(ROOT)/assets/font-atlas.bin
	@mkdir -p $(dir $@)
	cp $< $@

$(STAGE)/assets/OFL.txt: $(ROOT)/assets/OFL.txt
	@mkdir -p $(dir $@)
	cp $< $@

$(STAGE)/assets/motors/%.eng: $(ROOT)/assets/motors/%.eng
	@mkdir -p $(dir $@)
	cp $< $@

$(STAGE)/sce_sys/icon0.png: $(ROOT)/assets/icon0.png
	@mkdir -p $(dir $@)
	cp $< $@

# Provided with the OpenOrbis sample. Do not commit or redistribute SDK blobs.
$(STAGE)/sce_sys/about/right.sprx: $(SDK)/samples/piglet/sce_sys/about/right.sprx
	@mkdir -p $(dir $@)
	cp $< $@

$(STAGE)/sce_sys/param.sfo: Makefile
	@mkdir -p $(dir $@)
	$(PKGTOOL) sfo_new $@
	$(PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value 1.00
	$(PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value gd
	$(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value $(CONTENT_ID)
	$(PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value 'EuloLab Rocket Studio'
	$(PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value BREW00242
	$(PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value 1.00

$(STAGE)/pkg.gp4: Makefile $(ROOT)/tools/repair_gp4.py $(STAGE)/eboot.bin $(STAGE)/sce_sys/param.sfo $(STAGE)/sce_sys/icon0.png $(STAGE)/sce_sys/about/right.sprx $(STAGE)/assets/font-atlas.bin $(STAGE)/assets/OFL.txt $(STAGE)/assets/motors/Klima_C6.eng $(STAGE)/assets/motors/Klima_D9.eng
	cd $(STAGE) && $(GP4) -out pkg.gp4 --content-id=$(CONTENT_ID) \
	  --files "eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx assets/font-atlas.bin assets/OFL.txt assets/motors/Klima_C6.eng assets/motors/Klima_D9.eng"
	python3 $(ROOT)/tools/repair_gp4.py $@ $(STAGE)

pkg: $(STAGE)/pkg.gp4
	cd $(STAGE) && $(PKGTOOL) pkg_build pkg.gp4 .
	@ls -lh $(STAGE)/$(CONTENT_ID).pkg

clean:
	rm -rf $(OUT)
