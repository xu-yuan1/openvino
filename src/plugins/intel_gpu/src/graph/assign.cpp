// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <assign_inst.h>
#include "primitive_type_base.h"
#include <sstream>
#include <json_object.h>
#include <data_inst.h>

namespace cldnn {
GPU_DEFINE_PRIMITIVE_TYPE_ID(assign)

assign_inst::typed_primitive_inst(network& network, const assign_node& node) :
    parent{network, node, false},
    memory_state::variable{node.get_primitive()->variable_id} {
}

layout assign_inst::calc_output_layout(const assign_node& node, kernel_impl_params const& impl_param) {
    return impl_param.typed_desc<assign>()->output_layout;
}

std::string assign_inst::to_string(const assign_node& node) {
    auto node_info = node.desc_to_json();
    json_composite assign_info;
    assign_info.add("input id", node.input().id());
    assign_info.add("variable id", node.get_primitive()->variable_id);
    node_info->add("assign info", assign_info);
    std::stringstream primitive_description;
    node_info->dump(primitive_description);
    return primitive_description.str();
}

void assign_inst::save(cldnn::BinaryOutputBuffer& ob) const {
    parent::save(ob);

    ob << variable_id();
}

void assign_inst::load(cldnn::BinaryInputBuffer& ib) {
    parent::load(ib);

    std::string variable_id;
    ib >> variable_id;
    set_variable_id(variable_id);
}
} // namespace cldnn
