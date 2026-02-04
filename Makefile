.PHONY: check-chipyard install-configs remove-configs apply-boom-patch revert-boom-patch \
        bitstream-nlp bitstream-tage bitstream-clean

check-chipyard:
	@if [ -z "$(CHIPYARD)" ]; then \
		echo "ERROR: CHIPYARD is not set. Run: export CHIPYARD=/path/to/chipyard"; \
		exit 1; \
	fi

apply-boom-patch: check-chipyard
	@test -d "$(BOOM_REPO)/.git" || (echo "ERROR: BOOM repo not found at $(BOOM_REPO) (did you init submodules?)" && exit 1)
	@if cd "$(BOOM_REPO)" && git apply --reverse --check "$(BOOM_PATCH)" >/dev/null 2>&1; then \
		echo "BOOM patch already applied"; \
	else \
		cd "$(BOOM_REPO)" && git apply --check "$(BOOM_PATCH)" && git apply "$(BOOM_PATCH)"; \
		echo "Applied BOOM patch: $(BOOM_PATCH)"; \
	fi

revert-boom-patch: check-chipyard
	@test -d "$(BOOM_REPO)/.git" || (echo "ERROR: BOOM repo not found at $(BOOM_REPO)" && exit 1)
	cd "$(BOOM_REPO)" && git apply -R "$(BOOM_PATCH)" || true
	@echo "Reverted BOOM patch (if applied)"

install-configs: check-chipyard
	mkdir -p "$(dir $(BOOM_DST))" "$(dir $(FPGA_DST))"
	cp -f "$(BOOM_SRC)" "$(BOOM_DST)"
	cp -f "$(FPGA_SRC)" "$(FPGA_DST)"
	@echo "Installed PRIM Scala configs into Chipyard"

remove-configs: check-chipyard
	rm -f "$(BOOM_DST)" "$(FPGA_DST)"
	@echo "Removed PRIM Scala configs from Chipyard (if present)"

bitstream-nlp: check-chipyard apply-boom-patch install-configs
	cd "$(CHIPYARD)" && source env.sh && cd fpga && \
	make SUB_PROJECT=nexysvideo \
		CONFIG=CustomNexysVideoNLPConfig \
		CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
		bitstream

bitstream-tage: check-chipyard apply-boom-patch install-configs
	cd "$(CHIPYARD)" && source env.sh && cd fpga && \
	make SUB_PROJECT=nexysvideo \
		CONFIG=CustomNexysVideoTAGEConfig \
		CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
		bitstream

bitstream-clean:
	$(MAKE) remove-configs
	$(MAKE) revert-boom-patch