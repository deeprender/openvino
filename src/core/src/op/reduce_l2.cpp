// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/op/reduce_l2.hpp"

#include "element_visitor.hpp"
#include "itt.hpp"
#include "openvino/op/util/axes_util.hpp"
#include "openvino/reference/reduce_l2.hpp"
#include "reduce_shape_inference.hpp"

namespace ov {
namespace op {

namespace reduce_l2 {
struct Evaluate : element::NoAction<bool> {
    using element::NoAction<bool>::visit;

    template <element::Type_t ET>
    static result_type visit(const Tensor& in0, Tensor& out, const AxisSet& reduction_axes) {
        using T = fundamental_type_for<ET>;
        reference::reduce_l2(in0.data<T>(), out.data<T>(), in0.get_shape(), reduction_axes);
        return true;
    }
};
}  // namespace reduce_l2
namespace v4 {

ReduceL2::ReduceL2(const Output<Node>& arg, const Output<Node>& reduction_axes, bool keep_dims)
    : ArithmeticReductionKeepDims(arg, reduction_axes, keep_dims) {
    constructor_validate_and_infer_types();
}

std::shared_ptr<Node> ReduceL2::clone_with_new_inputs(const OutputVector& new_args) const {
    OV_OP_SCOPE(v4_ReduceL2_clone_with_new_inputs);
    check_new_args_count(this, new_args);
    return std::make_shared<op::v4::ReduceL2>(new_args.at(0), new_args.at(1), get_keep_dims());
}

bool ReduceL2::evaluate(TensorVector& outputs, const TensorVector& inputs) const {
    OV_OP_SCOPE(v4_ReduceL2_evaluate);
    OPENVINO_ASSERT(outputs.size() == 1);
    OPENVINO_ASSERT(inputs.size() == 2);

    const auto reduction_axes = get_normalized_axes_from_tensor(this, inputs[1], inputs[0].get_shape().size());
    outputs[0].set_shape(ov::util::reduce(inputs[0].get_shape(), reduction_axes, get_keep_dims()));

    using namespace ov::element;
    return IfTypeOf<bf16, f16, f32>::apply<reduce_l2::Evaluate>(inputs[0].get_element_type(),
                                                                inputs[0],
                                                                outputs[0],
                                                                reduction_axes);
}

bool ReduceL2::has_evaluate() const {
    OV_OP_SCOPE(v4_ReduceL2_has_evaluate);
    switch (get_input_element_type(0)) {
    case element::bf16:
    case element::f16:
    case element::f32:
        return true;
    default:
        return false;
    }
}
}  // namespace v4
}  // namespace op
}  // namespace ov
