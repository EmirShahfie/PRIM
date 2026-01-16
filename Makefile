SHELL := /bin/bash

CHIPYARD ?= $(shell printenv CHIPYARD)

PRIM_DIR        := $(CURDIR)
PRIM_BUILDS     := $(PRIM_DIR)/demo-attacks/builds

TEST_DIR        := $(CHIPYARD)/tests
BUILD_DIR       := $(TEST_DIR)/build

UART_TSI        := $(CHIPYARD)/generators/testchipip/uart_tsi/uart_tsi
TTY             ?= /dev/ttyUSB0

# ---------- FPGA / Vivado config ----------
VIVADO          ?= vivado
FPGA_PROCS_DIR  := $(PRIM_DIR)/fpga-procs
BRAD_BIT        := $(FPGA_PROCS_DIR)/BradBoom.bit
SPECTRE_BIT     := $(FPGA_PROCS_DIR)/SpectreBoom.bit
TCL_SCRIPT      := program_fpga.tcl

.PHONY: all configure run-bradv1 smart-lock run-smart-lock flash-brad flash-spectre clean patch-apply patch-restore

configure :
	source "$(CHIPYARD)/env.sh" && \
	cd $(CHIPYARD)/tests && \
	cmake -S ./ -B ./build/ -D CMAKE_BUILD_TYPE=Debug 

build-bradv1 :
	source "$(CHIPYARD)/env.sh" && \
	cd $(CHIPYARD)/tests && \
	cmake --build ./build/ --target bradv1

run-bradv1:
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(BUILD_DIR)/bradv1.riscv"

smart-lock: 
	@set -e; \
	trap '$(MAKE) patch-restore' EXIT; \
	$(MAKE) patch-apply; \
	source "$(CHIPYARD)/env.sh" && \
	export PRIM_DIR="$(PRIM_DIR)" && \
	cmake --build "$(BUILD_DIR)" --target smart-lock && \
	cp "$(BUILD_DIR)/smart-lock.riscv" "$(PRIM_BUILDS)"; \
	:

run-smart-lock:
	source "$(CHIPYARD)/env.sh" && \
	"$(UART_TSI)" +tty=$(TTY) "$(BUILD_DIR)/smart-lock.riscv"

flash-brad: $(BRAD_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(BRAD_BIT)

flash-spectre: $(SPECTRE_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(SPECTRE_BIT)

clean:
	rm -f "$(PRIM_BUILDS)/bradv1.riscv" "$(PRIM_BUILDS)/smart-lock.riscv"