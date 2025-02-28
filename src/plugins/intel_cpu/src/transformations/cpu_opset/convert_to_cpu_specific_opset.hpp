// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <ngraph/pass/constant_folding.hpp>
#include "ngraph/op/fake_quantize.hpp"
#include "ngraph/pass/manager.hpp"
#include "common/pass/reshape_fc_fusion.hpp"
#include "common/pass/align_matmul_input_ranks.hpp"
#include "transformations/common_optimizations/reshape_prelu.hpp"
#include "common/pass/convert_broadcast_to_tiles.hpp"
#include "common/pass/convert_tile_to_seq_tiles.hpp"
#include "common/pass/convert_matmul_to_fc.hpp"
#include "common/pass/convert_to_power_static.hpp"
#include "common/pass/convert_to_leaky_relu.hpp"
#include "common/pass/convert_to_swish_cpu.hpp"
#include "common/pass/move_fc_reshape_to_weights.hpp"
#include "transformations/convert_precision.hpp"
#include "transformations/utils/utils.hpp"
#include "common/pass/rnn_sequences_optimization.hpp"
#include "transformations/common_optimizations/reshape_sequence_fusion.hpp"
#include "common/pass/ngram_fusion.hpp"
#include "transformations/defs.hpp"

#include "itt.hpp"

namespace ov {
namespace intel_cpu {

inline void ConvertToCPUSpecificOpset(std::shared_ptr<ngraph::Function> &nGraphFunc) {
    RUN_ON_FUNCTION_SCOPE(ConvertToCPUSpecificOpset);

    ngraph::pass::Manager manager;
    manager.set_per_pass_validation(false);
    CPU_REGISTER_PASS_COMMON(manager, ConvertMatMulToFC);
    CPU_REGISTER_PASS_X64(manager, MoveFCReshapeToWeights);
    CPU_REGISTER_PASS_X64(manager, ov::pass::Validate);
    CPU_REGISTER_PASS_COMMON(manager, AlignMatMulInputRanks);
    CPU_REGISTER_PASS_COMMON(manager, ConvertTileToSeqTiles);
    CPU_REGISTER_PASS_X64(manager, ConvertToPowerStatic);
    CPU_REGISTER_PASS_COMMON(manager, ConvertToLeakyRelu);
    CPU_REGISTER_PASS_COMMON(manager, ConvertToSwishCPU);
    CPU_REGISTER_PASS_COMMON(manager, OptimizeSequenceTransposes);
    if (!ov::op::util::has_op_with_type<ngraph::op::FakeQuantize>(nGraphFunc)) {
        CPU_REGISTER_PASS_COMMON(manager, ReshapeFullyConnectedFusion);
    }
    // after transformation "MoveEltwiseUpThroughDataMov" there can be reshaped sequences that should be eliminated or fused
    CPU_REGISTER_PASS_COMMON(manager, ov::pass::ReshapeSequenceFusion);
    CPU_REGISTER_PASS_COMMON(manager, ov::pass::ConstantFolding);
    CPU_REGISTER_PASS_COMMON(manager, ov::pass::ConvertPrecision, precisions_map {{ ngraph::element::i64, ngraph::element::i32 }});
    CPU_REGISTER_PASS_COMMON(manager, NgramFusion);
    CPU_REGISTER_PASS_COMMON(manager, ov::pass::Validate);

    manager.run_passes(nGraphFunc);
}

}   // namespace intel_cpu
}   // namespace ov
