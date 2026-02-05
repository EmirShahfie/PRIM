SHELL := /bin/bash

# =========================================================
# Paths
# =========================================================

PRIM_DIR        := $(CURDIR)
PRIM_BUILDS     := $(PRIM_DIR)/demo-attacks/builds
PRIM_CONFIGS    := $(PRIM_DIR)/boom-configs

CHIPYARD ?=

CONDA_ENV ?=

UART_TSI        := $(PRIM_DIR)/demo-attacks/uart_tsi
TTY             ?= /dev/ttyUSB0

# =========================================================
# BOOM patch + configs
# =========================================================

BOOM_PATCH := $(PRIM_CONFIGS)/boom_nlp.patch

BOOM_SRC := $(PRIM_CONFIGS)/CustomBoomConfigs.scala
FPGA_SRC := $(PRIM_CONFIGS)/CustomNexysVideoConfigs.scala

BOOM_REPO := $(CHIPYARD)/generators/boom

BOOM_DST := $(CHIPYARD)/generators/chipyard/src/main/scala/config/$(notdir $(BOOM_SRC))
FPGA_DST := $(CHIPYARD)/fpga/src/main/scala/nexysvideo/$(notdir $(FPGA_SRC))

# =========================================================
# Toolchain
# =========================================================

RISCV_PREFIX := riscv64-unknown-elf

SPECS    := htif_nano.specs
LDSCRIPT := $(CHIPYARD)/tests/htif.ld

CFLAGS   := -std=gnu99 -O2 -Wall -Wextra \
            -fno-common -fno-builtin-printf \
            -march=rv64imafd -mabi=lp64d -mcmodel=medany \
            -specs=$(SPECS)

LDFLAGS  := -static -T $(LDSCRIPT)

# =========================================================
# Bitstreams
# =========================================================

VIVADO          ?= vivado
FPGA_PROCS_DIR  := $(PRIM_DIR)/fpga-procs

BRAD_BIT        := $(FPGA_PROCS_DIR)/BradBoom.bit
SPECTRE_BIT     := $(FPGA_PROCS_DIR)/SpectreBoom.bit

TCL_SCRIPT      := program_fpga.tcl

# =========================================================
# Demo programs
# =========================================================

CBPA_SRC        := $(PRIM_DIR)/demo-attacks/CBPA.c
SMARTLOCK_SRC   := $(PRIM_DIR)/demo-attacks/smart-lock.c
IBPA_SRC        := $(PRIM_DIR)/demo-attacks/IBPA.c

CBPA_ELF        := $(PRIM_BUILDS)/CBPA.riscv
SMARTLOCK_ELF   := $(PRIM_BUILDS)/smart-lock.riscv
IBPA_ELF        := $(PRIM_BUILDS)/IBPA.riscv

CBPA_DUMP       := $(PRIM_BUILDS)/CBPA.dump
SMARTLOCK_DUMP  := $(PRIM_BUILDS)/smart-lock.dump
IBPA_DUMP       := $(PRIM_BUILDS)/IBPA.dump

# =========================================================
# Phony
# =========================================================

.PHONY: all \
	build-CBPA build-smart-lock build-IBPA \
	run-CBPA run-smart-lock run-IBPA \
	dump-CBPA dump-smart-lock dump-IBPA \
	flash-brad flash-spectre \
	clean \
	check-chipyard \
	check-conda-env \
	apply-boom-patch revert-boom-patch \
	install-configs remove-configs \
	compile-nlp compile-tage \
	bitstream-nlp bitstream-tage \
	bitstream-clean

# =========================================================
# Default
# =========================================================

all: build-CBPA build-smart-lock

$(PRIM_BUILDS):
	@mkdir -p "$@"

# =========================================================
# Safety check
# =========================================================

check-chipyard:
	@if [ -z "$(CHIPYARD)" ]; then \
		echo "ERROR: CHIPYARD not set"; \
		echo "Run: export CHIPYARD=/path/to/chipyard"; \
		exit 1; \
	fi

check-conda-env:
	@if [ -z "$(CONDA_ENV)" ]; then \
		echo "ERROR: CONDA_ENV not set"; \
		echo "Run: export CONDA_ENV=/path/to/conda.sh"; \
		exit 1; \
	fi

# =========================================================
# Patch management (idempotent)
# =========================================================

apply-boom-patch: check-chipyard check-conda-env
	@echo "Applying BOOM patch..."
	cd "$(BOOM_REPO)" && (git apply --check "$(BOOM_PATCH)" && git apply "$(BOOM_PATCH)")
	@echo "BOOM patch ready"

revert-boom-patch: check-chipyard check-conda-env
	@echo "Reverting BOOM patch..."
	cd "$(BOOM_REPO)" && git apply -R "$(BOOM_PATCH)" || true

# =========================================================
# Config copy
# =========================================================

install-configs: check-chipyard check-conda-env
	cp -f "$(BOOM_SRC)" "$(BOOM_DST)"
	cp -f "$(FPGA_SRC)" "$(FPGA_DST)"
	@echo "Configs installed"

remove-configs: check-chipyard check-conda-env
	rm -f "$(BOOM_DST)" "$(FPGA_DST)"

# =========================================================
# FAST TEST (Scala compile only)
# =========================================================

compile-nlp: apply-boom-patch install-configs
	cd "$(CHIPYARD)/fpga" && source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && \
	make SUB_PROJECT=nexysvideo \
		CONFIG=CustomNexysVideoNLPConfig \
		CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
		$(CHIPYARD)/.classpath_cache/chipyard_fpga.jar

compile-tage: apply-boom-patch install-configs
	cd "$(CHIPYARD)/fpga" && source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && \
	make SUB_PROJECT=nexysvideo \
		CONFIG=CustomNexysVideoTAGEConfig \
		CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
		$(CHIPYARD)/.classpath_cache/chipyard_fpga.jar

# =========================================================
# FULL FPGA BITSTREAM
# =========================================================

bitstream-nlp: apply-boom-patch install-configs
	cd "$(CHIPYARD)/fpga" && source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && \
	make SUB_PROJECT=nexysvideo \
		CONFIG=CustomNexysVideoNLPConfig \
		CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
		bitstream

bitstream-tage: apply-boom-patch install-configs
	cd "$(CHIPYARD)/fpga" && source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && \
	make SUB_PROJECT=nexysvideo \
		CONFIG=CustomNexysVideoTAGEConfig \
		CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
		bitstream

bitstream-clean:
	$(MAKE) remove-configs
	$(MAKE) revert-boom-patch

# =========================================================
# Demo builds
# =========================================================

build-CBPA: $(CBPA_ELF)
build-smart-lock: $(SMARTLOCK_ELF)
build-IBPA: $(IBPA_ELF)

$(CBPA_ELF): $(CBPA_SRC) | $(PRIM_BUILDS)
	source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(SMARTLOCK_ELF): $(SMARTLOCK_SRC) | $(PRIM_BUILDS)
	source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

$(IBPA_ELF): $(IBPA_SRC) | $(PRIM_BUILDS)
	source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && \
	$(RISCV_PREFIX)-gcc $(CFLAGS) $< $(LDFLAGS) -o $@

dump-CBPA: $(CBPA_DUMP)
dump-smart-lock: $(SMARTLOCK_DUMP)
dump-IBPA: $(IBPA_DUMP)

$(CBPA_DUMP): $(CBPA_ELF)
	$(RISCV_PREFIX)-objdump -D $< > $@

$(SMARTLOCK_DUMP): $(SMARTLOCK_ELF)
	$(RISCV_PREFIX)-objdump -D $< > $@

$(IBPA_DUMP): $(IBPA_ELF)
	$(RISCV_PREFIX)-objdump -D $< > $@

run-CBPA: $(CBPA_ELF)
	source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && "$(UART_TSI)" +tty=$(TTY) $<

run-smart-lock: $(SMARTLOCK_ELF)
	source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && "$(UART_TSI)" +tty=$(TTY) $<

run-IBPA: $(IBPA_ELF)
	source "$(CONDA_ENV)" && source "$(CHIPYARD)/env.sh" && "$(UART_TSI)" +tty=$(TTY) $<

# =========================================================
# Flash FPGA
# =========================================================

flash-brad:
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(BRAD_BIT)

flash-spectre:
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(SPECTRE_BIT)

# =========================================================
# Clean
# =========================================================

clean: revert-boom-patch remove-configs
	rm -f "$(PRIM_BUILDS)"/*.riscv "$(PRIM_BUILDS)"/*.dump