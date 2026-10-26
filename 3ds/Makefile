.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

APP_TITLE		:= 	Checkpoint
APP_DESCRIPTION	:=	Checkpoint JP: save manager
APP_AUTHOR		:=	Bernardo Giordano, FlagBrew

TARGET			:=	$(subst $e ,_,$(notdir $(APP_TITLE)))
OUTDIR			:=	out
BUILD			:=	build
FORMATSOURCES	:=	source source/script ../common ../common/script
SOURCES			:=	$(FORMATSOURCES) ../3rd-party/sha256 \
					../3rd-party/ftpd/source \
					../common/script/cstdlib \
					../3rd-party/picoc/source/interpreter
DATA			:=	data
FORMATINCLUDES	:=	include ../common ../common/script
INCLUDES		:=	$(FORMATINCLUDES) ../3rd-party/json ../3rd-party/sha256 ../3rd-party/ftpd/include \
					../3rd-party/picoc/include
GRAPHICS		:=	assets/gfx
ROMFS			:=	assets/romfs
GFXBUILD		:=	$(ROMFS)/gfx

SCRIPTSRC		:=	../scripts
ROMFS_SCRIPTS	:=	$(ROMFS)/scripts

MONO_TTF		:=	../switch/romfs/fonts/SpaceMono-Regular.ttf
MONO_BCFNT		:=	$(ROMFS)/fonts/SpaceMono.bcfnt

MONO_SIZE		:=	7

ICON			:=	assets/icon.png

BANNER_AUDIO	:=	assets/audio.wav
BANNER_IMAGE	:=	assets/banner.png
RSF_PATH		:=	assets/app.rsf

LOGO			:=	assets/splash.bin

UNIQUE_ID		:=	0xBCFFF
PRODUCT_CODE	:=	CTR-HB-CKPT

ICON_FLAGS          :=	nosavebackups,visible

ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -gdwarf-4 -Wall -Wextra -Wno-psabi -O3 -mword-relocations -flto=auto \
			-fomit-frame-pointer -ffunction-sections \
			-Wno-unused-parameter \
			$(ARCH) \
			-DVERSION_MAJOR=${VERSION_MAJOR} \
			-DVERSION_MINOR=${VERSION_MINOR} \
			-DVERSION_MICRO=${VERSION_MICRO} \
			-DGIT_REV=\"${GIT_REV}\" \
			-DJSON_HAS_FILESYSTEM=0 \
			-DJSON_HAS_EXPERIMENTAL_FILESYSTEM=0

CFLAGS	+=	$(INCLUDE) -DARM11 -D__3DS__ -D_GNU_SOURCE=1

CFLAGS	+=	-DUNIX_HOST -DDEBUGGER

CFLAGS	+=	-funwind-tables

CXXFLAGS	:= $(CFLAGS) -fno-rtti -std=gnu++23

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro2d -lcitro3d -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lctru -lm

CCACHE	:=	$(shell command -v ccache 2>/dev/null)
ifneq ($(strip $(CCACHE)),)
CXX		:=	$(CCACHE) $(CXX)
CC		:=	$(CCACHE) $(CC)
endif

LIBDIRS	:= $(CTRULIB) $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(OUTDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))

CFILES		:=	$(filter-out debug.c,$(CFILES))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)

	export LD	:=	$(CC)

else

	export LD	:=	$(CXX)

endif

export T3XFILES		:=	$(GFXFILES:.t3s=.t3x)

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) \
			$(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
			$(if $(filter $(BUILD),$(GFXBUILD)),$(addsuffix .o,$(T3XFILES)))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(OUTDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all binaries buildelf directories clean format

ROMFS_SPRITES	:=	$(GFXBUILD)/sprites.t3x

ROMFS_STATIC	:=	$(shell find $(ROMFS) -type f -not -path "$(GFXBUILD)/*" -not -path "$(ROMFS_SCRIPTS)/*" -not -path "$(dir $(MONO_BCFNT))*")
export ROMFS_DEPS	:=	$(ROMFS_SPRITES) $(MONO_BCFNT) $(ROMFS_SCRIPTS) $(ROMFS_STATIC)

MAKEFILE_DEPS	:=	$(CURDIR)/Makefile $(CURDIR)/../Makefile

all: binaries

binaries: $(OUTPUT).3dsx $(OUTPUT).cia

$(ROMFS_STATIC): ;

directories:
	@mkdir -p $(BUILD) $(GFXBUILD) $(OUTDIR)

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS) $(ROMFS_DEPS) | directories
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).3dsx

$(OUTPUT).cia: $(OUTPUT).elf $(BUILD)/banner.bnr $(BUILD)/icon.icn $(LOGO) $(RSF_PATH) $(ROMFS_DEPS) $(MAKEFILE_DEPS) | directories
	@makerom -f cia -o $(OUTPUT).cia -target t -exefslogo -elf "$(OUTPUT).elf" -rsf "$(RSF_PATH)" -ver "$$(($(VERSION_MAJOR)*1024+$(VERSION_MINOR)*16+$(VERSION_MICRO)))" -banner "$(BUILD)/banner.bnr" -icon "$(BUILD)/icon.icn" -DAPP_TITLE="$(APP_TITLE)" -DAPP_PRODUCT_CODE="$(PRODUCT_CODE)" -DAPP_UNIQUE_ID="$(UNIQUE_ID)" -logo "$(LOGO)"
	@echo built ... $(notdir $@)

$(OUTPUT).elf: buildelf | directories
	@true

buildelf: $(ROMFS_SPRITES) | directories
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile buildelf

$(BUILD)/banner.bnr: $(BANNER_IMAGE) $(BANNER_AUDIO) | directories
	@bannertool makebanner -i "$(BANNER_IMAGE)" -a "$(BANNER_AUDIO)" -o $@

$(BUILD)/icon.icn: $(APP_ICON) $(MAKEFILE_DEPS) | directories
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i "$(APP_ICON)" -f "$(ICON_FLAGS)" -o $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(GFXBUILD) $(OUTDIR) $(MONO_BCFNT) $(ROMFS_SCRIPTS)

$(MONO_BCFNT): $(MONO_TTF) $(CURDIR)/Makefile
	@mkdir -p $(dir $@)
	@mkbcfnt -s $(MONO_SIZE) -o $@ $<

$(ROMFS_SCRIPTS): $(shell find $(SCRIPTSRC)/common $(SCRIPTSRC)/3ds -type f)
	@rm -fr $@
	@mkdir -p $@
	@cp -R $(SCRIPTSRC)/common/. $@
	@cp -R $(SCRIPTSRC)/3ds/. $@
	@touch $@

$(ROMFS_SPRITES): $(wildcard $(GRAPHICS)/*.t3s) $(wildcard $(GRAPHICS)/*.png) | directories
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile sprites.t3x
	@cp $(BUILD)/sprites.t3x $@

format:
	clang-format -i -style=file $(foreach dir,$(FORMATSOURCES),$(wildcard $(dir)/*.c) $(wildcard $(dir)/*.cpp) $(wildcard $(dir)/*.tcc)) $(foreach dir,$(FORMATINCLUDES),$(wildcard $(dir)/*.h) $(wildcard $(dir)/*.hpp))

else

$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS) $(ROMFS_DEPS:%=../%)

$(ROMFS_DEPS:%=../%): ;

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

.PHONY: buildelf
buildelf: $(OUTPUT).elf
	@:

%.bin.o	%_bin.h :	%.bin

	@echo $(notdir $<)
	@$(bin2o)

%.t3x.o	%_t3x.h :	%.t3x

	@$(bin2o)

define shader-as
	$(eval CURBIN := $*.shbin)
	$(eval DEPSFILE := $(DEPSDIR)/$*.shbin.d)
	echo "$(CURBIN).o: $< $1" > $(DEPSFILE)
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u32" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(CURBIN) | tr . _)`.h
	picasso -o $(CURBIN) $1
	bin2s $(CURBIN) | $(AS) -o $*.shbin.o
endef

%.shbin.o %_shbin.h : %.v.pica %.g.pica
	@echo $(notdir $^)
	@$(call shader-as,$^)

%.shbin.o %_shbin.h : %.v.pica
	@echo $(notdir $<)
	@$(call shader-as,$<)

%.shbin.o %_shbin.h : %.shlist
	@echo $(notdir $<)
	@$(call shader-as,$(foreach file,$(shell cat $<),$(dir $<)$(file)))

-include $(DEPSDIR)/*.d

endif
