// ============================================================
// File: CustomNexysVideoConfigs.scala
//
// Purpose:
//   FPGA binding for the PRIM BOOM configuration.
//   Maps BradBoomV3Config onto the Nexys Video board.
//
// This file should ONLY contain board-level composition.
// Do NOT put BOOM microarchitecture params here.
// ============================================================

package chipyard.fpga.nexysvideo

import org.chipsalliance.cde.config.Config

import chipyard.config.BradBoomV3Config
import chipyard.fpga.nexysvideo.WithNexysVideoTweaks
import chipyard.config.WithBroadcastManager

/**
  * Nexys Video FPGA configuration using the PRIM BOOM core.
  *
  * Usage (fpga/Makefile):
  *   CONFIG ?= CustomNexysVideoConfig
  *   CONFIG_PACKAGE ?= chipyard.fpga.nexysvideo
  */
class CustomNexysVideoConfig extends Config(
  new WithNexysVideoTweaks ++      // clocks, DDR, IOs for Nexys
  new WithBroadcastManager ++     // required memory system tweak
  new BradBoomV3Config()          // your BOOM SoC
)