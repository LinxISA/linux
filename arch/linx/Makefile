# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#

OBJCOPYFLAGS    := -O binary
LDFLAGS_vmlinux :=
ifeq ($(CONFIG_DYNAMIC_FTRACE),y)
	LDFLAGS_vmlinux := --no-relax
	KBUILD_CPPFLAGS += -DCC_USING_PATCHABLE_FUNCTION_ENTRY
	CC_FLAGS_FTRACE := -fpatchable-function-entry=8
endif

export BITS
ifeq ($(CONFIG_ARCH_RV64I),y)
	BITS := 64
	UTS_MACHINE := linx64

	KBUILD_LDFLAGS += -melf64linx
else
	BITS := 32
	UTS_MACHINE := linx32
	KBUILD_LDFLAGS += -melf32linx
endif

ifeq ($(CONFIG_LD_IS_LLD),y)
ifndef CONFIG_AS_IS_LLVM
	KBUILD_CFLAGS += -Wa,-mno-relax
	KBUILD_AFLAGS += -Wa,-mno-relax
endif
endif

# ISA string setting
linx-march-$(CONFIG_ARCH_RV32I)	:= linx32
linx-march-$(CONFIG_ARCH_RV64I)	:= linx64
linx-march-$(CONFIG_FPU)		:= $(linx-march-y)fd
linx-march-$(CONFIG_RISCV_ISA_C)	:= $(linx-march-y)c
KBUILD_CFLAGS += -march=$(subst fd,,$(linx-march-y))
KBUILD_AFLAGS += -march=$(linx-march-y)

KBUILD_CFLAGS += -DCONFIG_PAGE_OFFSET=$(CONFIG_PAGE_OFFSET)
ifeq ($(CONFIG_PERF_EVENTS),y)
        KBUILD_CFLAGS += -fno-omit-frame-pointer
endif

# GCC versions that support the "-mstrict-align" option default to allowing
# unaligned accesses.  While unaligned accesses are explicitly allowed in the
# RISC-V ISA, they're emulated by machine mode traps on all extant
# architectures.  It's faster to have GCC emit only aligned accesses.

ifeq ($(CONFIG_STACKPROTECTOR_PER_TASK),y)
prepare: stack_protector_prepare
stack_protector_prepare: prepare0
	$(eval KBUILD_CFLAGS += -mstack-protector-guard=tls		  \
				-mstack-protector-guard-reg=tp		  \
				-mstack-protector-guard-offset=$(shell	  \
			awk '{if ($$2 == "TSK_STACK_CANARY") print $$3;}' \
					include/generated/asm-offsets.h))
endif

# arch specific predefines for sparse
CHECKFLAGS += -D__linx -D__riscv_xlen=$(BITS)

# Default target when executing plain make
boot		:= arch/linx/boot
ifeq ($(CONFIG_XIP_KERNEL),y)
KBUILD_IMAGE := $(boot)/xipImage
else
KBUILD_IMAGE	:= $(boot)/Image.gz
endif

head-y := arch/linx/kernel/head.o

libs-y += arch/linx/lib/
libs-$(CONFIG_EFI_STUB) += $(objtree)/drivers/firmware/efi/libstub/lib.a

PHONY += vdso_install
vdso_install:
	$(Q)$(MAKE) $(build)=arch/linx/kernel/vdso $@

ifeq ($(CONFIG_MMU),y)
prepare: vdso_prepare
vdso_prepare: prepare0
	$(Q)$(MAKE) $(build)=arch/linx/kernel/vdso include/generated/vdso-offsets.h
endif

ifneq ($(CONFIG_XIP_KERNEL),y)
ifeq ($(CONFIG_RISCV_M_MODE)$(CONFIG_SOC_CANAAN),yy)
KBUILD_IMAGE := $(boot)/loader.bin
else
KBUILD_IMAGE := $(boot)/Image.gz
endif
endif
BOOT_TARGETS := Image Image.gz loader loader.bin xipImage

all:	$(notdir $(KBUILD_IMAGE))

$(BOOT_TARGETS): vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@
	@$(kecho) '  Kernel: $(boot)/$@ is ready'

Image.%: Image
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

install: install-image = Image
zinstall: install-image = Image.gz
install zinstall:
	$(CONFIG_SHELL) $(srctree)/$(boot)/install.sh $(KERNELRELEASE) \
	$(boot)/$(install-image) System.map "$(INSTALL_PATH)"

PHONY += rv32_randconfig
rv32_randconfig:
	$(Q)$(MAKE) KCONFIG_ALLCONFIG=$(srctree)/arch/linx/configs/32-bit.config \
		-f $(srctree)/Makefile randconfig

PHONY += rv64_randconfig
rv64_randconfig:
	$(Q)$(MAKE) KCONFIG_ALLCONFIG=$(srctree)/arch/linx/configs/64-bit.config \
		-f $(srctree)/Makefile randconfig
