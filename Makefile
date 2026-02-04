SHELL := /bin/bash

PRIM_DIR        := $(CURDIR)
PRIM_BUILDS     := $(PRIM_DIR)/demo-attacks/builds
PRIM_CONFIGS    := $(PRIM_DIR)/boom-configs

UART_TSI        := $(CHIPYARD)/generators/testchipip/uart_tsi/uart_tsi
TTY             ?= /dev/ttyUSB0

BOOM_SRC := $(PRIM_CONFIGS)/CustomBoomConfigs.scala
FPGA_SRC := $(PRIM_CONFIGS)/CustomNexysVideoConfigs.scala

BOOM_DST := $(CHIPYARD)/generators/chipyard/src/main/scala/config/$(notdir $(BOOM_SRC))
FPGA_DST := $(CHIPYARD)/fpga/src/main/scala/nexysvideo/configs/$(notdir $(FPGA_SRC))

# Fixed typo: riscv64-unknown-el -> riscv64-unknown-elf
RISCV_PREFIX := riscv64-unknown-elf

# Make specs path explicit (more robust than relying on cwd)
SPECS    := $(CHIPYARD)/tests/htif_nano.specs

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

CBPA_SRC        := $(PRIM_DIR)/demo-attacks/CBPA.c
SMARTLOCK_SRC   := $(PRIM_DIR)/demo-attacks/smart-lock.c
IBPA_SRC        := $(PRIM_DIR)/demo-attacks/IBPA.c

CBPA_ELF        := $(PRIM_BUILDS)/CBPA.riscv
SMARTLOCK_ELF   := $(PRIM_BUILDS)/smart-lock.riscv
IBPA_ELF        := $(PRIM_BUILDS)/IBPA.riscv

CBPA_DUMP       := $(PRIM_BUILDS)/CBPA.dump
SMARTLOCK_DUMP  := $(PRIM_BUILDS)/smart-lock.dump
IBPA_DUMP       := $(PRIM_BUILDS)/IBPA.dump

.PHONY: all build-CBPA build-smart-lock build-IBPA run-CBPA run-smart-lock run-IBPA \
        dump-CBPA dump-smart-lock dump-IBPA flash-brad flash-spectre clean \
        check-chipyard install-configs remove-configs bitstream-nexys bitstream-nexys-clean

all: build-CBPA build-smart-lock

$(PRIM_BUILDS):
	@mkdir -p "$@"

# -----------------------------
# PRIM <-> Chipyard config sync
# -----------------------------

check-chipyard:
	@if [ -z "$(CHIPYARD)" ]; then \
		echo "ERROR: CHIPYARD is not set. Run: export CHIPYARD=/path/to/chipyard"; \
		exit 1; \
	fi

# Copy the Scala config files into Chipyard (overwrites, so no duplicates)
install-configs: check-chipyard
	cp -f "$(BOOM_SRC)" "$(BOOM_DST)"
	cp -f "$(FPGA_SRC)" "$(FPGA_DST)"
	@echo "Installed PRIM Scala configs into Chipyard: $(notdir $(BOOM_SRC)), $(notdir $(FPGA_SRC))"

# Remove the Scala config files from Chipyard if present
remove-configs: check-chipyard
	rm -f "$(BOOM_DST)"
	rm -f "$(FPGA_DST)"
	@echo "Removed PRIM Scala configs from Chipyard (if present)"

# Build NexysVideo bitstream using Option A (override CONFIG on command line)
# Assumes PrimNexysVideoConfig is defined under package chipyard.fpga.nexysvideo
bitstream-nexys: check-chipyard install-configs
	cd "$(CHIPYARD)" && source env.sh && cd fpga && \
	make SUB_PROJECT=nexysvideo \
		CONFIG=PrimNexysVideoConfig \
		CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
		bitstream
	@echo "Bitstream build complete (NexysVideo, PrimNexysVideoConfig)"

# Convenience: build bitstream then cleanly remove copied files
bitstream-nexys-clean: bitstream-nexys
	$(MAKE) remove-configs

# -----------------------------
# Demo builds
# -----------------------------

build-CBPA: $(CBPA_ELF)
build-smart-lock: $(SMARTLOCK_ELF)
build-IBPA: $(IBPA_ELF)

$(CBPA_ELF): $(CBPA_SRC) | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(SMARTLOCK_ELF): $(SMARTLOCK_SRC) | $(PRIM_BUILDS)
	source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(IBPA_ELF): $(IBPA_SRC) | $(PRIM_BUILDS)
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
	rm -f "$(CBPA_ELF)" "$(SMARTLOCK_ELF)" "$(IBPA_ELF)" \
	      "$(CBPA_DUMP)" "$(SMARTLOCK_DUMP)" "$(IBPA_DUMP)"