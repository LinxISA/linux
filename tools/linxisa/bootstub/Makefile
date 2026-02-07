LLVM_BUILD ?= $(HOME)/llvm-project/build-linxisa-clang
CLANG ?= $(LLVM_BUILD)/bin/clang
LLD ?= $(LLVM_BUILD)/bin/ld.lld

TARGET ?= linx64-unknown-elf
OUT_DIR ?= build

COMMON_CFLAGS := -O2 -ffreestanding -fno-builtin -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -fno-exceptions \
	-fno-jump-tables

.PHONY: all elf clean

all: $(OUT_DIR)/bootstub.o

# Optional: link an ET_EXEC image (useful once the LinxISA linker relaxations
# and PC-relative relocation ranges are fully validated).
elf: $(OUT_DIR)/bootstub.elf

$(OUT_DIR):
	@mkdir -p "$@"

$(OUT_DIR)/bootstub.o: bootstub.c | $(OUT_DIR)
	"$(CLANG)" -target "$(TARGET)" $(COMMON_CFLAGS) -c "$<" -o "$@"

$(OUT_DIR)/bootstub.elf: $(OUT_DIR)/bootstub.o linker.ld | $(OUT_DIR)
	"$(LLD)" -e _start -T linker.ld -o "$@" "$<"

clean:
	rm -rf "$(OUT_DIR)"
