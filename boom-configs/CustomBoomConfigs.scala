// File: chipyard/generators/chipyard/src/main/scala/config/CustomBoomConfigs.scala
//
// Purpose:
//   - Defines a BOOM-based Chipyard SoC config in ONE public class (BradBoomV3Config)
//   - Keeps BOOM micro-arch params (core/tile) and Chipyard composition in the same file
//
// Notes:
//   - You still *compose* a small Config fragment inside the class (that’s how CDE Config works).
//   - This file is Chipyard-side (NOT fpga-side). FPGA configs should just reference BradBoomV3Config.

package chipyard.config

import org.chipsalliance.cde.config.Config

import freechips.rocketchip.subsystem.{InSubsystem, NumTiles, TilesLocated}
import freechips.rocketchip.subsystem.RocketCrossingParams
import freechips.rocketchip.tile.FPUParams

import boom.v3.common._
import boom.v3.issue.{IssueParams, IQT_FP, IQT_INT, IQT_MEM}

/**
  * A compact BOOM-based Chipyard config intended to fit on small FPGA targets (e.g., Nexys Video).
  *
  * @param n Number of BOOM tiles to attach (default 1).
  */
class BradBoomV3Config(n: Int = 1) extends Config(
  // Branch predictor / NLP mixin (as in your original)
  new WithNLPBimBtbBPD ++

  // Attach BOOM tiles into the subsystem
  new Config((site, here, up) => {
    case TilesLocated(InSubsystem) =>
      val prev = up(TilesLocated(InSubsystem), site)

      (0 until n).map { _ =>
        BoomTileAttachParams(
          tileParams = BoomTileParams(
            core = BoomCoreParams(
              // --- Frontend / pipeline sizing ---
              fetchWidth = 4,
              decodeWidth = 1,

              // --- OoO structures ---
              numRobEntries = 32,

              // --- Issue queues (3 IQs, all 1-wide, 8 entries) ---
              issueParams = Seq(
                IssueParams(
                  issueWidth = 1,
                  numEntries = 8,
                  iqType = IQT_MEM.litValue,
                  dispatchWidth = 1
                ),
                IssueParams(
                  issueWidth = 1,
                  numEntries = 8,
                  iqType = IQT_INT.litValue,
                  dispatchWidth = 1
                ),
                IssueParams(
                  issueWidth = 1,
                  numEntries = 8,
                  iqType = IQT_FP.litValue,
                  dispatchWidth = 1
                )
              ),

              // --- Physical register files ---
              numIntPhysRegisters = 52,
              numFpPhysRegisters  = 48,

              // --- LSU queues ---
              numLdqEntries = 8,
              numStqEntries = 8,

              // --- Branch / fetch tracking ---
              maxBrCount            = 8,
              numFetchBufferEntries = 8,
              ftq                   = FtqParameters(nEntries = 16),

              // --- Perf counters (keep low for area) ---
              nPerfCounters = 2,

              // --- FPU (optional; you can remove to save resources) ---
              fpu = Some(FPUParams(sfmaLatency = 4, dfmaLatency = 4, divSqrt = true))
            )
          ),

          // Keep the default Rocket-style crossing
          crossingParams = RocketCrossingParams()
        )
      } ++ prev

    case NumTiles =>
      up(NumTiles) + n
  }) ++

  // Chipyard baseline (buses, peripherals, memory system defaults, etc.)
  new chipyard.config.AbstractConfig
)