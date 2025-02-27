// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "decoder_flatbuffer.h"
#include "place.hpp"
#include "quantization_info.hpp"
#include "schema_generated.h"

namespace ov {
namespace frontend {
namespace tensorflow_lite {

class TensorLitePlace;
class QuantizationInfo;

ov::element::Type get_ov_type(const tflite::TensorType& tf_type);
ov::PartialShape get_ov_shape(const flatbuffers::Vector<int32_t>* tf_shape);
std::shared_ptr<QuantizationInfo> get_quantization(const tflite::QuantizationParameters* tf_quantization);
void apply_quantization(ov::Output<ov::Node>& output);

}  // namespace tensorflow_lite
}  // namespace frontend
}  // namespace ov