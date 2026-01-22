SHELL := /bin/bash

PRIM_DIR        := $(CURDIR)
PRIM_BUILDS     := $(PRIM_DIR)/demo-attacks/builds

UART_TSI        := $(CHIPYARD)/generators/testchipip/uart_tsi/uart_tsi
TTY             ?= /dev/ttyUSB0

RISCV_PREFIX := riscv64-unknown-elf

SPECS    := htif_nano.specs

CFLAGS   := -std=gnu99 -O2 -Wall -Wextra \
            -fno-common -fno-builtin-printf \
            -march=rv64imafd -mabi=lp64d -mcmodel=medany \
            -specs=$(SPECS)

# Use chipyard's htif.ld from tests (adjust if yours is elsewhere)
LDSCRIPT := $(CHIPYARD)/tests/htif.ld
LDFLAGS  := -static -T $(LDSCRIPT)

# ---------- FPGA / Vivado config ----------
VIVADO          ?= vivado
FPGA_PROCS_DIR  := $(PRIM_DIR)/fpga-procs
BRAD_BIT        := $(FPGA_PROCS_DIR)/BradBoom.bit
SPECTRE_BIT     := $(FPGA_PROCS_DIR)/SpectreBoom.bit
TCL_SCRIPT      := program_fpga.tcl

# ----- Apps -----
BRADV1_SRC      := $(PRIM_DIR)/demo-attacks/bradv1.c
SMARTLOCK_SRC   := $(PRIM_DIR)/demo-attacks/smart-lock.c

BRADV1_ELF      := $(PRIM_BUILDS)/bradv1.riscv
SMARTLOCK_ELF   := $(PRIM_BUILDS)/smart-lock.riscv
PINK_ELF        := $(PRIM_BUILDS)/pink.riscv

BRADV1_DUMP     := $(PRIM_BUILDS)/bradv1.dump
SMARTLOCK_DUMP  := $(PRIM_BUILDS)/smart-lock.dump
PINK_DUMP      := $(PRIM_BUILDS)/pink.dump

.PHONY: all build-bradv1 build-smart-lock run-bradv1 run-smart-lock \
        dump-bradv1 dump-smart-lock dump-pink flash-brad flash-spectre clean

all: build-bradv1 build-smart-lock

# Ensure output dir exists
$(PRIM_BUILDS):
	@mkdir -p "$@"

# ---------- Build ----------
build-bradv1: $(BRADV1_ELF)
build-smart-lock: $(SMARTLOCK_ELF)
build-pink: $(PINK_ELF)

$(BRADV1_ELF): $(BRADV1_SRC) | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(SMARTLOCK_ELF): $(SMARTLOCK_SRC) | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(PINK_ELF): $(PRIM_DIR)/demo-attacks/pink.c | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

# ---------- Dump ----------
dump-bradv1: $(BRADV1_DUMP)
dump-smart-lock: $(SMARTLOCK_DUMP)
dump-pink: $(PINK_DUMP)

$(BRADV1_DUMP): $(BRADV1_ELF) | $(PRIM_BUILDS)
	$(RISCV_PREFIX)-objdump -D $< > $@

$(SMARTLOCK_DUMP): $(SMARTLOCK_ELF) | $(PRIM_BUILDS)
	$(RISCV_PREFIX)-objdump -D $< > $@

$(PINK_DUMP): $(PINK_ELF) | $(PRIM_BUILDS)
	$(RISCV_PREFIX)-objdump -D $< > $@

# ---------- Run ----------
run-bradv1: $(BRADV1_ELF)
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(BRADV1_ELF)"

run-smart-lock: $(SMARTLOCK_ELF)
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(SMARTLOCK_ELF)"

run-pink: $(PINK_ELF)
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(PINK_ELF)"

# ---------- FPGA flash ----------
flash-brad: $(BRAD_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(BRAD_BIT)

flash-spectre: $(SPECTRE_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(SPECTRE_BIT)

# ---------- Clean ----------
clean:
	rm -f "$(BRADV1_ELF)" "$(SMARTLOCK_ELF)" "$(BRADV1_DUMP)" "$(SMARTLOCK_DUMP)" "$(PINK_ELF)" "$(PINK_DUMP)"