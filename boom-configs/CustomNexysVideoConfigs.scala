// ============================================================================
// File: CustomNexysVideoConfigs.scala
// Put this in your PRIM repo (e.g. PRIM/boom-configs/CustomNexysVideoConfigs.scala)
// Copy destination in Chipyard:
//   $(CHIPYARD)/fpga/src/main/scala/nexysvideo/configs/CustomNexysVideoConfigs.scala
//   (OR the actual NexysVideo configs folder in your Chipyard checkout)
//
// Build example (Option A):
//   make SUB_PROJECT=nexysvideo \
//        CONFIG=CustomNexysVideoNLPConfig \
//        CONFIG_PACKAGE=chipyard.fpga.nexysvideo \
//        bitstream
// ============================================================================

package chipyard.fpga.nexysvideo

import org.chipsalliance.cde.config.Config
import chipyard.{CustomBoomV3NLPConfig, CustomBoomV3TAGEConfig}
import chipyard.config.WithBroadcastManager

// Nexys Video + BOOM V3 (NLP predictor variant)
class CustomNexysVideoNLPConfig extends Config(
  new WithNexysVideoTweaks ++
  new WithBroadcastManager ++
  new CustomBoomV3NLPConfig
)

// Nexys Video + BOOM V3 (Small TAGE predictor variant)
class CustomNexysVideoTAGEConfig extends Config(
  new WithNexysVideoTweaks ++
  new WithBroadcastManager ++
  new CustomBoomV3TAGEConfig
)