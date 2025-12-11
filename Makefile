TEST_DIR        = /home/users/mbinmohd-23/chipyard/tests
BUILD_DIR       = /home/users/mbinmohd-23/chipyard/tests/build
PRIM_DIR        = /home/users/mbinmohd-23/PRIM/demo-attacks/builds
UART_TSI        = /home/users/mbinmohd-23/chipyard/generators/testchipip/uart_tsi/uart_tsi

# ---------- FPGA / Vivado config ----------
VIVADO          ?= vivado
FPGA_PROCS_DIR  = /home/users/mbinmohd-23/PRIM/fpga-procs

BRAD_BIT        = $(FPGA_PROCS_DIR)/BradBoom.bit
SPECTRE_BIT     = $(FPGA_PROCS_DIR)/SpectreBoom.bit
TCL_SCRIPT      = program_fpga.tcl

.PHONY: all bradv1 clean flash-brad flash-spectre flash

all: bradv1

bradv1:
	cd $(TEST_DIR) && \
	cmake --build $(BUILD_DIR) --target bradv1 && \
	cp $(BUILD_DIR)/bradv1.riscv $(PRIM_DIR)

run-bradv1:
	$(UART_TSI) +tty=/dev/ttyUSB0 $(BUILD_DIR)/bradv1.riscv

smart-lock:
	cd $(TEST_DIR) && \
	cmake --build $(BUILD_DIR) --target smart-lock && \
	cp $(BUILD_DIR)/smart-lock.riscv $(PRIM_DIR)

run-smart-lock:
	$(UART_TSI) +tty=/dev/ttyUSB0 $(BUILD_DIR)/smart-lock.riscv

# Flash BradBoom.bit to the FPGA
flash-brad: $(BRAD_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(BRAD_BIT)

# Flash SpectreBoom.bit to the FPGA
flash-spectre: $(SPECTRE_BIT)
	$(VIVADO) -mode batch -source $(TCL_SCRIPT) -tclargs $(SPECTRE_BIT)

clean:
	rm -f $(PRIM_DIR)/bradv1.riscv
	rm -f $(PRIM_DIR)/smart-lock.riscv