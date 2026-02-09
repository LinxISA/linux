# SPDX-License-Identifier: GPL-2.0-only
obj-y	= printk.o
obj-$(CONFIG_PRINTK)	+= printk_safe.o nbcon.o
obj-$(CONFIG_A11Y_BRAILLE_CONSOLE)	+= braille.o
obj-$(CONFIG_PRINTK_INDEX)	+= index.o

obj-$(CONFIG_PRINTK)                 += printk_support.o
printk_support-y	             := printk_ringbuffer.o
printk_support-$(CONFIG_SYSCTL)	     += sysctl.o

obj-$(CONFIG_PRINTK_RINGBUFFER_KUNIT_TEST) += printk_ringbuffer_kunit_test.o

# LinxISA bring-up: clang backend still has issues at -O2 in printk ringbuffer
# codepaths (e.g. descriptor state bit constants clobbering base pointers).
# Build the ringbuffer with -O0 until the backend is fully fixed.
#
# IMPORTANT: Restrict to the Linx build only. Other architectures (e.g. RISC-V)
# rely on inlining for certain asm-constraint patterns and will fail to build
# if compiled at -O0.
ifeq ($(ARCH),linx)
CFLAGS_printk_ringbuffer.o += -Og
CFLAGS_printk.o += -Og
endif
