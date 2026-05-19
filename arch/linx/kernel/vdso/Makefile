# SPDX-License-Identifier: GPL-2.0-only
# Copied from arch/tile/kernel/vdso/Makefile

# Absolute relocation type $(ARCH_REL_TYPE_ABS) needs to be defined before
# the inclusion of generic Makefile.
ARCH_REL_TYPE_ABS := R_LINX_32|R_LINX_64|R_LINX_JUMP_SLOT
include $(srctree)/lib/vdso/Makefile.include
# Symbols present in the vdso
vdso-syms  = rt_sigreturn
ifdef CONFIG_64BIT
vdso-syms += vgettimeofday
endif
vdso-syms += getcpu
vdso-syms += flush_icache

# Files to link into the vdso
obj-vdso = $(patsubst %, %.o, $(vdso-syms)) note.o

ccflags-y := -fno-stack-protector
ccflags-y += -DCONFIG_PAGE_SHIFT=12

ifneq ($(c-gettimeofday-y),)
  CFLAGS_vgettimeofday.o += -fPIC -include $(c-gettimeofday-y)
endif

ifeq ($(strip $(c-gettimeofday-y)),)
  CFLAGS_vgettimeofday.o += -fPIC -include $(GENERIC_VDSO_DIR)gettimeofday.c
endif

# Build rules
targets := $(obj-vdso) vdso.so vdso.so.dbg vdso.lds
obj-vdso := $(addprefix $(obj)/, $(obj-vdso))

obj-y += vdso.o
CPPFLAGS_vdso.lds += -P -C -U$(ARCH)

# Disable -pg to prevent insert call site
CFLAGS_REMOVE_vgettimeofday.o = $(CC_FLAGS_FTRACE) -Os

# Disable profiling and instrumentation for VDSO code
GCOV_PROFILE := n
KCOV_INSTRUMENT := n
KASAN_SANITIZE := n
UBSAN_SANITIZE := n

# Force dependency
$(obj)/vdso.o: $(obj)/vdso.so

# link rule for the .so file, .lds has to be first
SYSCFLAGS_vdso.so.dbg = -shared -Wl,-S -Wl,-soname=linux-vdso.so.1 \
	-Wl,--build-id=sha1 -Wl,--hash-style=both -Wl,--eh-frame-hdr
$(obj)/vdso.so.dbg: $(obj)/vdso.lds $(obj-vdso) FORCE
	$(call if_changed,vdsold_and_check)

# strip rule for the .so file
$(obj)/%.so: OBJCOPYFLAGS := -S
$(obj)/%.so: $(obj)/%.so.dbg FORCE
	$(call if_changed,objcopy)

# Generate VDSO offsets using helper script
gen-vdsosym := $(src)/gen_vdso_offsets.sh
quiet_cmd_vdsosym = VDSOSYM $@
	cmd_vdsosym = $(NM) $(obj)/vdso.so.dbg | $(gen-vdsosym) | LC_ALL=C sort > $@

include/generated/vdso-offsets.h: $(obj)/vdso.so.dbg FORCE
	$(call if_changed,vdsosym)

# actual build commands
# The DSO images are built using a special linker script
# Make sure only to export the intended __vdso_xxx symbol offsets.
quiet_cmd_vdsold_and_check = VDSOLD  $@
      cmd_vdsold_and_check = $(CC) $(KBUILD_CFLAGS) $(call cc-option, -no-pie) -nostdlib -nostartfiles $(SYSCFLAGS_$(@F)) \
                           -Wl,-T,$(filter-out FORCE,$^) -o $@.tmp && \
                   $(OBJCOPY) $(patsubst %, -G __vdso_%, $(vdso-syms)) $@.tmp $@ && \
                   rm $@.tmp && \
                   $(cmd_vdso_check)

# install commands for the unstripped file
quiet_cmd_vdso_install = INSTALL $@
      cmd_vdso_install = cp $(obj)/$@.dbg $(MODLIB)/vdso/$@

vdso.so: $(obj)/vdso.so.dbg
	@mkdir -p $(MODLIB)/vdso
	$(call cmd,vdso_install)

vdso_install: vdso.so
