// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <array>

#include "intel_gpu/plugin/program_builder.hpp"
#include "intel_gpu/plugin/common_utils.hpp"

#include "openvino/op/matmul.hpp"
#include "openvino/op/constant.hpp"
#include "openvino/op/fake_quantize.hpp"

#include "intel_gpu/primitives/gemm.hpp"
#include "intel_gpu/primitives/fully_connected.hpp"
#include "intel_gpu/primitives/reshape.hpp"
#include "intel_gpu/primitives/reorder.hpp"
#include "intel_gpu/primitives/permute.hpp"

namespace ov {
namespace intel_gpu {

static void CreateMatMulOp(ProgramBuilder& p, const std::shared_ptr<ov::op::v0::MatMul>& op) {
    validate_inputs_count(op, {2});
    auto inputs = p.GetInputInfo(op);
    std::string layerName = layer_type_name_ID(op);

    auto shape_a = op->get_input_partial_shape(0);
    auto shape_b = op->get_input_partial_shape(1);

    auto rank_a = shape_a.rank().get_length();
    auto rank_b = shape_b.rank().get_length();

    auto alpha = 1.0f;
    auto beta = 0.0f;
    auto transA = op->get_transpose_a();
    auto transB = op->get_transpose_b();

    std::array<ov::PartialShape, 2> inputShapes{
        op->get_input_partial_shape(0),
        op->get_input_partial_shape(1)
    };

    auto canTransposeInputs = [&] (const std::array<ov::PartialShape, 2>& shapes, bool transA, bool transB, ov::element::Type type) -> bool {
        if (!transA && !transB)
            return false;

        // dynamic shapes and 1D tensors are not transposed
        if (shapes[0].is_dynamic() || shapes[1].is_dynamic() ||
            shapes[0].size() < 2 || shapes[1].size() < 2)
            return false;

        // don't transpose inputs if they're aligned to 16
        bool inputsAligned = std::all_of(shapes[0].rbegin(), shapes[0].rbegin() + 2,
                                            [] (const ov::Dimension& dim) { return dim.is_static() && dim.get_length() % 16 == 0; }) &&
                                std::all_of(shapes[1].rbegin(), shapes[1].rbegin() + 2,
                                            [] (const ov::Dimension& dim) { return dim.is_static() && dim.get_length() % 16 == 0; });
        if (inputsAligned)
            return false;

        // Heuristic condition for permute and tiled_opt kernel perform better than ref kernel.
        bool in0_large = std::all_of(shapes[0].rbegin(), shapes[0].rbegin() + 2,
                                    [] (const ov::Dimension& dim) { return dim.is_static() && dim.get_length() >= 64; });
        bool in1_large = std::all_of(shapes[1].rbegin(), shapes[1].rbegin() + 2,
                                    [] (const ov::Dimension& dim) { return dim.is_static() && dim.get_length() >= 64; });
        // Optimized for clDNN
        auto is_u8_i8 = (type == ov::element::Type_t::i8 || type == ov::element::Type_t::u8);
        bool in0_very_large = tensor_from_dims(shapes[0].to_shape()).count() > 100000;
        bool in1_very_large = tensor_from_dims(shapes[0].to_shape()).count() > 100000;
        bool needs_to_transpose_inputs = (in0_very_large || in1_very_large) && !is_u8_i8 && !p.get_engine().get_device_info().supports_immad;

        return (in0_large && in1_large) || needs_to_transpose_inputs;
    };

    auto transposeInput = [] (ProgramBuilder& p, const std::shared_ptr<ov::Node>& op, const ov::PartialShape& shape,
                                const std::string& suffix, const cldnn::primitive_id& primitiveId) -> cldnn::input_info {
        std::vector<uint16_t> transposeOrder(shape.size());
        std::iota(transposeOrder.begin(), transposeOrder.end(), 0);
        std::swap(*(transposeOrder.end() - 1), *(transposeOrder.end() - 2));

        auto permuteName = op->get_friendly_name() + suffix;
        auto permutePrim = cldnn::permute(permuteName,
                                            cldnn::input_info(primitiveId),
                                            transposeOrder);
        p.add_primitive(*op, permutePrim);
        return cldnn::input_info(permuteName);
    };

    if (canTransposeInputs(inputShapes, transA, transB, op->get_input_element_type(0))) {
        if (transA) {
            inputs[0] = transposeInput(p, op, inputShapes[0], "/transpose_a", inputs[0].pid);
            transA = false;
        }

        if (transB) {
            inputs[1] = transposeInput(p, op, inputShapes[1], "/transpose_b", inputs[1].pid);
            transB = false;
        }
    }

    auto gemmPrim = cldnn::gemm(layerName,
                                inputs,
                                cldnn::element_type_to_data_type(op->get_output_element_type(0)),
                                transA,
                                transB,
                                alpha,
                                beta,
                                rank_a,
                                rank_b);

    p.add_primitive(*op, gemmPrim);

    if (!p.use_new_shape_infer()) {
        auto outDims = op->get_output_shape(0);
        auto outDimsN = outDims.size();
        // Reshape output if gemm specific shape does not match default one
        if (outDimsN < 4) {
            auto outputShape = tensor_from_dims(outDims);
            auto outReshapeName = layerName + "_cldnn_out_reshape";
            auto outReshapePrim = cldnn::reshape(outReshapeName, cldnn::input_info(layerName), outputShape);
            p.add_primitive(*op, outReshapePrim);
        }
    }
}

REGISTER_FACTORY_IMPL(v0, MatMul);

}  // namespace intel_gpu
}  // namespace ov
