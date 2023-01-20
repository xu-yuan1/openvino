// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>

#include "default_opset.hpp"
#include "ngraph/node.hpp"
#include "ngraph/op/asin.hpp"
#include "onnx_import/core/node.hpp"

namespace ngraph {
namespace onnx_import {
namespace op {
namespace set_1 {
inline OutputVector asin(const Node& node) {
    return {std::make_shared<default_opset::Asin>(node.get_ng_inputs().at(0))};
}
}  // namespace set_1

}  // namespace op

}  // namespace onnx_import

}  // namespace ngraph
