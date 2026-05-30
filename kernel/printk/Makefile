# SPDX-License-Identifier: GPL-2.0-only
obj-y	= printk.o
obj-$(CONFIG_PRINTK)	+= printk_safe.o nbcon.o
obj-$(CONFIG_A11Y_BRAILLE_CONSOLE)	+= braille.o
obj-$(CONFIG_PRINTK_INDEX)	+= index.o

obj-$(CONFIG_PRINTK)                 += printk_support.o
printk_support-y	             := printk_ringbuffer.o
printk_support-$(CONFIG_SYSCTL)	     += sysctl.o

obj-$(CONFIG_PRINTK_RINGBUFFER_KUNIT_TEST) += printk_ringbuffer_kunit_test.o

# LinxISA bring-up: printk ringbuffer still stresses the backend, but -O0
# leaves atomic instrumentation helpers out of line and produces broken value
# flow in desc_reserve(). Keep printk.c conservative, but let the ringbuffer
# inline the tiny access helpers again.
#
# IMPORTANT: Restrict to the Linx build only. Other architectures (e.g. RISC-V)
# rely on inlining for certain asm-constraint patterns and will fail to build
# if compiled with these Linx-only bring-up flags.
ifeq ($(ARCH),linx)
CFLAGS_printk_ringbuffer.o += -O1
CFLAGS_printk.o += -O0
KASAN_SANITIZE_printk_ringbuffer.o := n
KCSAN_SANITIZE_printk_ringbuffer.o := n
endif
