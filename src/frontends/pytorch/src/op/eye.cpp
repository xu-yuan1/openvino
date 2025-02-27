// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/op/eye.hpp"

#include "openvino/frontend/pytorch/node_context.hpp"
#include "openvino/op/constant.hpp"
#include "openvino/op/convert.hpp"
#include "utils.hpp"

namespace ov {
namespace frontend {
namespace pytorch {
namespace op {

using namespace ov::op;

OutputVector translate_eye(NodeContext& context) {
    size_t num_inputs = context.get_input_size();
    auto x = context.get_input(0);
    // num rows and cols should be integer, but at the moment conversion their data type can be unknown yet
    x = context.mark_node(std::make_shared<v0::Convert>(x, element::i64));
    Output<Node> y;
    int dtype_id;
    auto dtype = element::f32;
    // aten::eye support only main diagonal
    auto diagonal = context.mark_node(v0::Constant::create(element::i64, Shape{}, {0}));
    if (num_inputs == 5) {
        // aten::eye(n, dtype, layout, device, pin_memory)
        y = x;
        dtype_id = 1;
    } else if (num_inputs == 6) {
        // aten::eye(n, m, dtype, layout, device, pin_memory)
        y = context.get_input(1);
        y = context.mark_node(std::make_shared<v0::Convert>(y, element::i64));
        dtype_id = 2;
    } else {
        FRONT_END_OP_CONVERSION_CHECK(false, "Unsupported number of inputs: ", num_inputs, " for aten::eye");
    }
    if (!context.input_is_none(dtype_id)) {
        dtype = convert_dtype(context.const_input<int64_t>(dtype_id));
    }
    auto eye = context.mark_node(std::make_shared<v9::Eye>(x, y, diagonal, element::i32));
    return {context.mark_node(std::make_shared<v0::Convert>(eye, dtype))};
};

}  // namespace op
}  // namespace pytorch
}  // namespace frontend
}  // namespace ov