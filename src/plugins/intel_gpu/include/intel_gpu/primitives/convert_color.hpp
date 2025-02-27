// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "primitive.hpp"
#include "intel_gpu/runtime/memory_caps.hpp"

namespace cldnn {

/// @brief Performs image conversion from one format to another
struct convert_color : public primitive_base<convert_color> {
    CLDNN_DECLARE_PRIMITIVE(convert_color)

    enum color_format : uint32_t {
        RGB,       ///< RGB color format
        BGR,       ///< BGR color format, default in OpenVINO
        RGBX,      ///< RGBX color format with X ignored during inference
        BGRX,      ///< BGRX color format with X ignored during inference
        NV12,      ///< NV12 color format represented as compound Y+UV blob
        I420,      ///< I420 color format represented as compound Y+U+V blob
    };

    enum memory_type : uint32_t {
        buffer,
        image
    };

    /// @brief Constructs convert_color primitive.
    /// @param id This primitive id.
    /// @param inputs Input primitives ids.
    /// @param input_color_format Color to convert from.
    /// @param output_color_format Color to convert to.
    /// @param mem_type Memory type.
    /// @param output_layout Requested memory layout.
    convert_color(const primitive_id& id,
                  const std::vector<input_info>& inputs,
                  const color_format input_color_format,
                  const color_format output_color_format,
                  const memory_type mem_type,
                  const layout& output_layout,
                  const padding& output_padding = padding())
        : primitive_base(id, inputs, {output_padding}),
          input_color_format(input_color_format),
          output_color_format(output_color_format),
          mem_type(mem_type),
          output_layout(output_layout) {}

    color_format input_color_format;
    color_format output_color_format;
    memory_type mem_type;
    layout output_layout;

    size_t hash() const override {
        size_t seed = primitive::hash();
        seed = hash_combine(seed, input_color_format);
        seed = hash_combine(seed, output_color_format);
        seed = hash_combine(seed, mem_type);
        return seed;
    }
};
}  // namespace cldnn
