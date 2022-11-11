// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <transformations_visibility.hpp>

#include "openvino/op/util/multiclass_nms_base.hpp"
#include "openvino/opsets/opset9.hpp"

namespace ov {
namespace op {
namespace internal {

class TRANSFORMATIONS_API MulticlassNmsIEInternal : public opset9::MulticlassNms {
public:
    OPENVINO_OP("MulticlassNmsIEInternal", "ie_internal_opset", opset9::MulticlassNms);
    BWDCMP_RTTI_DECLARATION;

    MulticlassNmsIEInternal() = default;

    MulticlassNmsIEInternal(const Output<Node>& boxes,
                            const Output<Node>& scores,
                            const op::util::MulticlassNmsBase::Attributes& attrs);

    MulticlassNmsIEInternal(const Output<Node>& boxes,
                            const Output<Node>& scores,
                            const Output<Node>& roisnum,
                            const op::util::MulticlassNmsBase::Attributes& attrs);

    void validate_and_infer_types() override;

    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;
};
}  // namespace internal
}  // namespace op
}  // namespace ov

namespace ngraph {
namespace op {
namespace internal {
using ov::op::internal::MulticlassNmsIEInternal;
}  // namespace internal
}  // namespace op
}  // namespace ngraph
