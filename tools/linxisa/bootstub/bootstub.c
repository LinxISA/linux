#include <stddef.h>
#include <stdint.h>

#define LINX_VIRT_UART_BASE 0x10000000u

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static void uart_putc(char c)
{
    while ((mmio_read32((uintptr_t)LINX_VIRT_UART_BASE + 0x4) & 1u) == 0) {
    }
    mmio_write32((uintptr_t)LINX_VIRT_UART_BASE + 0x0, (uint32_t)(uint8_t)c);
}

static void uart_puts(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s);
    }
}

static void uart_put_hex_u64(uint64_t v)
{
    static const char hexdig[] = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 15; i >= 0; i--) {
        uart_putc(hexdig[(v >> (i * 4)) & 0xf]);
    }
}

static __attribute__((noreturn)) void virt_exit(uint32_t code)
{
    mmio_write32((uintptr_t)LINX_VIRT_UART_BASE + 0x4, code);
    for (;;) {
    }
}

static uint32_t read_be32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) |
           (uint32_t)b[3];
}

static uint64_t read_be64(const void *p)
{
    return ((uint64_t)read_be32(p) << 32) | (uint64_t)read_be32((const uint8_t *)p + 4);
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int str_has_prefix(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static const uint8_t *align4(const uint8_t *p)
{
    return (const uint8_t *)(((uintptr_t)p + 3u) & ~(uintptr_t)3u);
}

enum {
    FDT_MAGIC = 0xd00dfeedu,
};

enum {
    FDT_BEGIN_NODE = 1,
    FDT_END_NODE = 2,
    FDT_PROP = 3,
    FDT_NOP = 4,
    FDT_END = 9,
};

static void fdt_dump_basic(uintptr_t fdt_addr)
{
    const uint8_t *fdt = (const uint8_t *)fdt_addr;

    uint32_t magic = read_be32(fdt + 0x00);
    uint32_t totalsize = read_be32(fdt + 0x04);
    uint32_t off_struct = read_be32(fdt + 0x08);
    uint32_t off_strings = read_be32(fdt + 0x0c);
    uint32_t version = read_be32(fdt + 0x14);

    uart_puts("fdt.magic=");
    uart_put_hex_u64((uint64_t)magic);
    uart_puts(" totalsize=");
    uart_put_hex_u64((uint64_t)totalsize);
    uart_puts(" off_struct=");
    uart_put_hex_u64((uint64_t)off_struct);
    uart_puts(" off_strings=");
    uart_put_hex_u64((uint64_t)off_strings);
    uart_puts(" version=");
    uart_put_hex_u64((uint64_t)version);
    uart_puts("\n");
}

static void fdt_scan_chosen_and_memory(uintptr_t fdt_addr)
{
    const uint8_t *fdt = (const uint8_t *)fdt_addr;
    uint32_t magic = read_be32(fdt + 0x00);

    if (magic != FDT_MAGIC) {
        uart_puts("error: bad fdt magic\n");
        return;
    }

    uint32_t off_struct = read_be32(fdt + 0x08);
    uint32_t off_strings = read_be32(fdt + 0x0c);
    uint32_t size_strings = read_be32(fdt + 0x20);
    uint32_t size_struct = read_be32(fdt + 0x24);

    const uint8_t *p = fdt + off_struct;
    const uint8_t *struct_end = p + size_struct;
    const uint8_t *strings = fdt + off_strings;
    const uint8_t *strings_end = strings + size_strings;

    int depth = 0;
    int chosen_depth = -1;
    int memory_depth = -1;

    uint64_t mem_addr = 0;
    uint64_t mem_size = 0;
    uint64_t initrd_start = 0;
    uint64_t initrd_end = 0;
    int have_mem = 0;
    int have_initrd = 0;

    while (p < struct_end) {
        uint32_t token = read_be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            while (p < struct_end && *p != '\0') {
                p++;
            }
            if (p >= struct_end) {
                uart_puts("error: truncated fdt\n");
                return;
            }
            p++;
            p = align4(p);

            depth++;
            if (depth == 2 && str_eq(name, "chosen")) {
                chosen_depth = depth;
            }
            if (depth == 2 && str_has_prefix(name, "memory")) {
                memory_depth = depth;
            }
            continue;
        }

        if (token == FDT_END_NODE) {
            if (depth == chosen_depth) {
                chosen_depth = -1;
            }
            if (depth == memory_depth) {
                memory_depth = -1;
            }
            depth--;
            continue;
        }

        if (token == FDT_PROP) {
            uint32_t len = read_be32(p);
            uint32_t nameoff = read_be32(p + 4);
            p += 8;

            const uint8_t *val = p;
            p += len;
            p = align4(p);

            if (strings + nameoff >= strings_end) {
                uart_puts("error: bad fdt strings offset\n");
                return;
            }
            const char *pname = (const char *)(strings + nameoff);

            if (depth == chosen_depth) {
                if (str_eq(pname, "bootargs") && len > 0) {
                    uart_puts("chosen.bootargs=");
                    uart_puts((const char *)val);
                    uart_puts("\n");
                } else if (str_eq(pname, "stdout-path") && len > 0) {
                    uart_puts("chosen.stdout-path=");
                    uart_puts((const char *)val);
                    uart_puts("\n");
                } else if (str_eq(pname, "linux,initrd-start") && len >= 4) {
                    initrd_start = (len >= 8) ? read_be64(val) : (uint64_t)read_be32(val);
                    have_initrd = 1;
                } else if (str_eq(pname, "linux,initrd-end") && len >= 4) {
                    initrd_end = (len >= 8) ? read_be64(val) : (uint64_t)read_be32(val);
                    have_initrd = 1;
                }
            } else if (depth == memory_depth && str_eq(pname, "reg") && len >= 16) {
                mem_addr = ((uint64_t)read_be32(val) << 32) | (uint64_t)read_be32(val + 4);
                mem_size = ((uint64_t)read_be32(val + 8) << 32) | (uint64_t)read_be32(val + 12);
                have_mem = 1;
            }
            continue;
        }

        if (token == FDT_NOP) {
            continue;
        }

        if (token == FDT_END) {
            goto out;
        }

        uart_puts("error: unknown fdt token\n");
        return;
    }

out:
    if (have_mem) {
        uart_puts("memory.reg.addr=");
        uart_put_hex_u64(mem_addr);
        uart_puts(" size=");
        uart_put_hex_u64(mem_size);
        uart_puts("\n");
    } else {
        uart_puts("memory.reg not found\n");
    }

    if (have_initrd) {
        uart_puts("chosen.initrd.start=");
        uart_put_hex_u64(initrd_start);
        uart_puts(" end=");
        uart_put_hex_u64(initrd_end);
        uart_puts("\n");
    }
}

int _start(uint64_t hartid, uintptr_t fdt_addr)
{
    uart_puts("\n[LinxISA] linux bootstub\n");
    uart_puts("hartid=");
    uart_put_hex_u64(hartid);
    uart_puts(" fdt=");
    uart_put_hex_u64((uint64_t)fdt_addr);
    uart_puts("\n");

    if (fdt_addr) {
        fdt_dump_basic(fdt_addr);
        fdt_scan_chosen_and_memory(fdt_addr);
    } else {
        uart_puts("no fdt provided\n");
    }

    uart_puts("[done] exiting\n");
    virt_exit(0);
}
