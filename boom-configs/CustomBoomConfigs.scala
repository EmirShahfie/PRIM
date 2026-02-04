// ============================================================================
// File: CustomBoomConfigs.scala
// Put this in your PRIM repo (e.g. PRIM/boom-configs/CustomBoomConfigs.scala)
// Copy destination in Chipyard:
//   $(CHIPYARD)/generators/chipyard/src/main/scala/config/CustomBoomConfigs.scala
//
// This assumes your BOOM patch adds these BOOM-side mixins:
//   - boom.v3.common.WithNSmallBoomsNLP
//   - boom.v3.common.WithNSmallBoomsTAGE
// ============================================================================

package chipyard

import org.chipsalliance.cde.config.Config

// BOOM V3 + NLP (micro-BTB) predictor chain
class CustomBoomV3NLPConfig extends Config(
  new boom.v3.common.WithNSmallBoomsNLP(1) ++
  new chipyard.config.AbstractConfig
)

// BOOM V3 + Small TAGE-L predictor (your reduced TAGE chain)
class CustomBoomV3TAGEConfig extends Config(
  new boom.v3.common.WithNSmallBoomsTAGE(1) ++
  new chipyard.config.AbstractConfig
)