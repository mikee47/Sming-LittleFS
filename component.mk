COMPONENT_SUBMODULES := littlefs
COMPONENT_SRCDIRS := src littlefs
COMPONENT_INCDIRS := src/include

COMPONENT_CFLAGS := -Wno-unused-function

HWCONFIG_BUILDSPECS += $(COMPONENT_PATH)/build.json

LFS_TOOLS := $(COMPONENT_PATH)/tools
LFSCOPY_TOOL := $(LFS_TOOLS)/fscopy/out/Host/release/firmware/fscopy$(TOOL_EXT)
LFSCOPY := $(LFSCOPY_TOOL) --nonet --debug=0 --

$(LFSCOPY_TOOL):
	$(Q) $(MAKE) -C $(LFS_TOOLS)/fscopy SMING_ARCH=Host SMING_RELEASE=1 ENABLE_CUSTOM_LWIP=2

# Target invoked via partition table
ifneq (,$(filter lfs-build,$(MAKECMDGOALS)))
PART_TARGET := $(PARTITION_$(PART)_FILENAME)
ifneq (,$(PART_TARGET))
$(eval PART_CONFIG := $(call HwExpr,part.build['config']))
.PHONY: lfs-build
lfs-build: $(LFSCOPY_TOOL)
	@echo "Creating intermediate FWFS image..."
	$(Q) $(FSBUILD) -i "$(subst ",\",$(PART_CONFIG))" -o $(PART_TARGET).fwfs
	@echo "Creating LFS image '$(PART_TARGET)'"
	$(Q) $(LFSCOPY) $(PART_TARGET).fwfs $(PART_TARGET).tmp $(PARTITION_$(PART)_SIZE_BYTES)
	$(Q) mv $(PART_TARGET).tmp $(PART_TARGET)
endif
endif
