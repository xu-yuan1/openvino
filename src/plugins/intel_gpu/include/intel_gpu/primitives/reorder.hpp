// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "primitive.hpp"
#include "intel_gpu/runtime/memory.hpp"
#include <vector>

namespace cldnn {

/// @brief reorder mean operation modes
enum class reorder_mean_mode {
    none,      // val
    subtract,  // val - mean
    mul,       // val * mean
    div,       // val/mean
};

/// @brief Changes how data is ordered in memory. Value type is not changed & all information is preserved.
/// @details Corresponding values are bitwise equal before/after reorder.
/// Also merged with subtraction layer, which can subtract, multiply or divide values based on mean_mode value, while doing reordering.
/// NOTE THAT THIS WILL SUBTRACT THE SAME VALUES FROM EACH BATCH.
struct reorder : public primitive_base<reorder> {
    CLDNN_DECLARE_PRIMITIVE(reorder)

    /// @brief reorder memory types
    enum class memory_type {
        buffer,
        surface
    };

    /// @brief Constructs reorder primitive with directly provided mean subtract values.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param values_to_subtract Array of mean subtract values.
    reorder(const primitive_id& id,
            const input_info& input,
            const layout& output_layout,
            const std::vector<float>& values_to_subtract = {},
            const reorder_mean_mode mode = reorder_mean_mode::subtract)
        : primitive_base(id, {input}, {output_layout.data_padding}, {optional_data_type {output_layout.data_type}}),
          output_format(output_layout.format),
          mean(""),
          subtract_per_feature(values_to_subtract),
          mean_mode(mode) {}

    /// @brief Constructs reorder primitive which takes mean subtract values from another primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param mean Primitive id to get mean subtract values.
    reorder(const primitive_id& id,
            const input_info& input,
            const layout& output_layout,
            primitive_id const& mean,
            const reorder_mean_mode mode = reorder_mean_mode::subtract)
        : primitive_base(id, {input}, {output_layout.data_padding}, {optional_data_type {output_layout.data_type}}),
          output_format(output_layout.format),
          mean(mean),
          subtract_per_feature(0),
          mean_mode(mode) {}

    /// @brief Constructs reorder primitive with directly provided mean subtract values.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param values_to_subtract Array of mean subtract values.
    /// @param truncate Convert truncation mode.
    reorder(const primitive_id& id,
            const input_info& input,
            format output_format,
            data_types output_data_type,
            const std::vector<float>& values_to_subtract = {},
            const reorder_mean_mode mode = reorder_mean_mode::subtract,
            const padding& output_padding = padding(),
            const bool truncate = false)
        : primitive_base(id, {input}, {output_padding}, {optional_data_type{output_data_type}}),
          output_format(output_format),
          mean(""),
          subtract_per_feature(values_to_subtract),
          mean_mode(mode),
          truncate(truncate) {}

    /// @brief Constructs reorder primitive which takes mean subtract values from another primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param mean Primitive id to get mean subtract values.
    reorder(const primitive_id& id,
            const input_info& input,
            format output_format,
            data_types output_data_type,
            primitive_id const& mean,
            const reorder_mean_mode mode = reorder_mean_mode::subtract,
            const padding& output_padding = padding())
        : primitive_base(id, {input}, {output_padding}, {optional_data_type {output_data_type}}),
          output_format(output_format),
          mean(mean),
          subtract_per_feature(0),
          mean_mode(mode) {}

    /// @brief Constructs reorder primitive with two inputs and directly provided mean subtract values.
    /// @param id This primitive id.
    /// @param input input primitive id.
    /// @param input input2 primitive id.
    /// @param output_layout Requested memory layout.
    /// @param values_to_subtract Array of mean subtract values.
    reorder(const primitive_id& id,
            const input_info& input,
            const input_info& input2,
            const layout& output_layout,
            const std::vector<float>& values_to_subtract = {},
            const reorder_mean_mode mode = reorder_mean_mode::subtract)
        : primitive_base(id, { input, input2 }, {output_layout.data_padding}, {optional_data_type { output_layout.data_type }}),
          output_format(output_layout.format),
          mean(""),
          subtract_per_feature(values_to_subtract),
          mean_mode(mode) {}

    /// @brief Constructs reorder primitive with two inputs, which takes mean subtract values from another primitive.
    /// @param id This primitive id.
    /// @param input input primitive id.
    /// @param input input2 primitive id.
    /// @param output_layout Requested memory layout.
    /// @param mean Primitive id to get mean subtract values.
    reorder(const primitive_id& id,
            const input_info& input,
            const input_info& input2,
            const layout& output_layout,
            primitive_id const& mean,
            const reorder_mean_mode mode = reorder_mean_mode::subtract)
        : primitive_base(id, { input, input2 }, {output_layout.data_padding}, {optional_data_type{ output_layout.data_type }}),
        output_format(output_layout.format),
        mean(mean),
        mean_mode(mode) {}

    /// @brief Requested memory format.
    format output_format;
    /// @brief Primitive id to get mean subtract values. Ignored if subtract_per_featrue is set.
    primitive_id mean;
    /// @brief Array of mean subtract values.
    std::vector<float> subtract_per_feature;
    /// @brief Mode of mean execution
    reorder_mean_mode mean_mode;
    /// @brief Input memory type
    memory_type input_mem_type = memory_type::buffer;

    inline bool has_surface_input() const {
        return input.size() == 1 &&
               input_mem_type == memory_type::surface;
    }

    /// @brief Convert truncation Mode
    bool truncate = false;

    size_t hash() const override {
        size_t seed = primitive::hash();
        seed = hash_combine(seed, mean_mode);
        seed = hash_combine(seed, input_mem_type);
        seed = hash_combine(seed, truncate);
        seed = hash_range(seed, subtract_per_feature.begin(), subtract_per_feature.end());
        seed = hash_combine(seed, mean.empty());
        return seed;
    }

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override {
        if (mean.empty())
            return {};
        return {mean};
    }
};

}  // namespace cldnn
