// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "mha.hpp"
#include "../itt.hpp"
#include <ngraph/opsets/opset3.hpp>
#include <matmul_shape_inference.hpp>

ov::intel_cpu::MHANode::MHANode(const ngraph::Output<ngraph::Node> &in0,
                                const ngraph::Output<ngraph::Node> &in1,
                                const ngraph::Output<ngraph::Node> &in2,
                                const ngraph::Output<ngraph::Node> &in3,
                                const std::vector<float> &mul_scales,
                                bool is_mul_first,
                                const ngraph::element::Type output_type)
    : Op({in0, in1, in2, in3}), m_output_type(output_type) {
    this->mul_scales = mul_scales;
    this->is_mul_first = is_mul_first;
    this->fq0_output_type = ngraph::element::undefined;
    this->fq1_output_type = ngraph::element::undefined;
    this->fq2_output_type = ngraph::element::undefined;
    validate_and_infer_types();
}

ov::intel_cpu::MHANode::MHANode(const ngraph::Output<ngraph::Node> &in0,
                                const ngraph::Output<ngraph::Node> &in1,
                                const ngraph::Output<ngraph::Node> &in2,
                                const ngraph::Output<ngraph::Node> &in3,
                                const std::vector<float> &mul_scales,
                                bool is_mul_first,
                                const std::vector<float> &fq_scales0,
                                const std::vector<float> &fq_scales1,
                                const std::vector<float> &fq_scales2,
                                const std::vector<float> &fq_scales3,
                                const ngraph::element::Type fq0_output_type,
                                const ngraph::element::Type fq1_output_type,
                                const ngraph::element::Type fq2_output_type,
                                const ngraph::element::Type output_type)
    : Op({in0, in1, in2, in3}), m_output_type(output_type) {
    this->mul_scales = mul_scales;
    this->is_mul_first = is_mul_first;
    this->fq_scales0 = fq_scales0;
    this->fq_scales1 = fq_scales1;
    this->fq_scales2 = fq_scales2;
    this->fq_scales3 = fq_scales3;
    this->fq0_output_type = fq0_output_type;
    this->fq1_output_type = fq1_output_type;
    this->fq2_output_type = fq2_output_type;
    validate_and_infer_types();
}

std::shared_ptr<ngraph::Node> ov::intel_cpu::MHANode::clone_with_new_inputs(const ngraph::OutputVector& new_args) const {
    INTERNAL_OP_SCOPE(MHANode_clone_with_new_inputs);
    check_new_args_count(this, new_args);
    return std::make_shared<ov::intel_cpu::MHANode>(new_args.at(0), new_args.at(1), new_args.at(2), new_args.at(3),
                                                    mul_scales, is_mul_first, fq_scales0, fq_scales1, fq_scales2, fq_scales3,
                                                    fq0_output_type, fq1_output_type, fq2_output_type, m_output_type);
}

void ov::intel_cpu::MHANode::validate_and_infer_types() {
    INTERNAL_OP_SCOPE(MHANode_validate_and_infer_types);

    auto transpose = [](const ov::Shape& shape, const std::vector<size_t>& order) -> ov::Shape {
        std::vector<size_t> new_shape(shape.size());
        for (int i = 0; i < shape.size(); i++) {
            new_shape[i] = shape[order[i]];
        }
        return new_shape;
    };

    const auto matmul0_shape0 = transpose(get_input_partial_shape(0).get_shape(), {0, 2, 1, 3});
    const auto matmul0_shape1 = transpose(get_input_partial_shape(1).get_shape(), {0, 2, 3, 1});

    auto matmul0_in0 = std::make_shared<ngraph::opset3::Parameter>(ngraph::element::f32, matmul0_shape0);
    auto matmul0_in1 = std::make_shared<ngraph::opset3::Parameter>(ngraph::element::f32, matmul0_shape1);
    auto matmul0 = std::make_shared<ngraph::opset3::MatMul>(matmul0_in0, matmul0_in1);

    std::vector<ov::PartialShape> matmul0_input_shapes = {matmul0_shape0, matmul0_shape1};
    std::vector<ov::PartialShape> matmul0_output_shapes = {ov::PartialShape{}};

    shape_infer(matmul0.get(), matmul0_input_shapes, matmul0_output_shapes);

    const auto matmul1_shape0 = matmul0_output_shapes[0];
    const auto matmul1_shape1 = transpose(get_input_partial_shape(3).get_shape(), {0, 2, 1, 3});

    auto matmul1_in0 = std::make_shared<ngraph::opset3::Parameter>(ngraph::element::f32, matmul1_shape0);
    auto matmul1_in1 = std::make_shared<ngraph::opset3::Parameter>(ngraph::element::f32, matmul1_shape1);
    auto matmul1 = std::make_shared<ngraph::opset3::MatMul>(matmul1_in0, matmul1_in1);

    std::vector<ov::PartialShape> matmul1_input_shapes = {matmul1_shape0, matmul1_shape1};
    std::vector<ov::PartialShape> matmul1_output_shapes = {ov::PartialShape{}};

    shape_infer(matmul1.get(), matmul1_input_shapes, matmul1_output_shapes);

    const auto output_shape = transpose(matmul1_output_shapes[0].get_shape(), {0, 2, 1, 3});

    set_output_type(
        0,
        m_output_type == ngraph::element::undefined || m_output_type == ngraph::element::dynamic ? get_input_element_type(0) : m_output_type,
        output_shape);
}

bool ov::intel_cpu::MHANode::visit_attributes(ngraph::AttributeVisitor &visitor) {
    INTERNAL_OP_SCOPE(MHANode_visit_attributes);
    visitor.on_attribute("out-type", m_output_type);
    return true;
}
