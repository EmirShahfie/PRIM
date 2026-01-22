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

LDSCRIPT := $(CHIPYARD)/tests/htif.ld
LDFLAGS  := -static -T $(LDSCRIPT)

VIVADO          ?= vivado
FPGA_PROCS_DIR  := $(PRIM_DIR)/fpga-procs
BRAD_BIT        := $(FPGA_PROCS_DIR)/BradBoom.bit
SPECTRE_BIT     := $(FPGA_PROCS_DIR)/SpectreBoom.bit
TCL_SCRIPT      := program_fpga.tcl

CBPA_SRC      := $(PRIM_DIR)/demo-attacks/CBPA.c
SMARTLOCK_SRC   := $(PRIM_DIR)/demo-attacks/smart-lock.c

CBPA_ELF      := $(PRIM_BUILDS)/CBPA.riscv
SMARTLOCK_ELF   := $(PRIM_BUILDS)/smart-lock.riscv
IBPA_ELF        := $(PRIM_BUILDS)/IBPA.riscv

CBPA_DUMP     := $(PRIM_BUILDS)/CBPA.dump
SMARTLOCK_DUMP  := $(PRIM_BUILDS)/smart-lock.dump
IBPA_DUMP      := $(PRIM_BUILDS)/IBPA.dump

.PHONY: all build-CBPA build-smart-lock run-CBPA run-smart-lock \
        dump-CBPA dump-smart-lock dump-IBPA flash-brad flash-spectre clean

all: build-CBPA build-smart-lock

$(PRIM_BUILDS):
	@mkdir -p "$@"

build-CBPA: $(CBPA_ELF)
build-smart-lock: $(SMARTLOCK_ELF)
build-IBPA: $(IBPA_ELF)

$(CBPA_ELF): $(CBPA_SRC) | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(SMARTLOCK_ELF): $(SMARTLOCK_SRC) | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(IBPA_ELF): $(PRIM_DIR)/demo-attacks/IBPA.c | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

dump-CBPA: $(CBPA_DUMP)
dump-smart-lock: $(SMARTLOCK_DUMP)
dump-IBPA: $(IBPA_DUMP)

$(CBPA_DUMP): $(CBPA_ELF) | $(PRIM_BUILDS)
	$(RISCV_PREFIX)-objdump -D $< > $@

$(SMARTLOCK_DUMP): $(SMARTLOCK_ELF) | $(PRIM_BUILDS)
	$(RISCV_PREFIX)-objdump -D $< > $@

$(IBPA_DUMP): $(IBPA_ELF) | $(PRIM_BUILDS)
	$(RISCV_PREFIX)-objdump -D $< > $@

run-CBPA: $(CBPA_ELF)
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(CBPA_ELF)"

run-smart-lock: $(SMARTLOCK_ELF)
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(SMARTLOCK_ELF)"

run-IBPA: $(IBPA_ELF)
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(IBPA_ELF)"

flash-brad: $(BRAD_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(BRAD_BIT)

flash-spectre: $(SPECTRE_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(SPECTRE_BIT)

clean:
	rm -f "$(CBPA_ELF)" "$(SMARTLOCK_ELF)" "$(CBPA_DUMP)" "$(SMARTLOCK_DUMP)" "$(IBPA_ELF)" "$(IBPA_DUMP)"