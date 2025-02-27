﻿// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "gather_nonzero_kernel_ref.h"
#include "kernel_selector_utils.h"
#include <string>

namespace kernel_selector {
ParamsKey GatherNonzeroKernelRef::GetSupportedKey() const {
    ParamsKey k;
    k.EnableInputDataType(Datatype::F16);
    k.EnableInputDataType(Datatype::F32);
    k.EnableInputDataType(Datatype::INT8);
    k.EnableInputDataType(Datatype::UINT8);
    k.EnableInputDataType(Datatype::INT32);
    k.EnableInputDataType(Datatype::UINT32);
    k.EnableInputDataType(Datatype::INT64);
    k.EnableOutputDataType(Datatype::F16);
    k.EnableOutputDataType(Datatype::F32);
    k.EnableOutputDataType(Datatype::UINT8);
    k.EnableOutputDataType(Datatype::INT8);
    k.EnableOutputDataType(Datatype::INT32);
    k.EnableOutputDataType(Datatype::UINT32);
    k.EnableOutputDataType(Datatype::INT64);
    k.EnableInputLayout(DataLayout::bfyx);
    k.EnableOutputLayout(DataLayout::bfyx);
    k.EnableInputLayout(DataLayout::bfzyx);
    k.EnableOutputLayout(DataLayout::bfzyx);
    k.EnableInputLayout(DataLayout::bfwzyx);
    k.EnableOutputLayout(DataLayout::bfwzyx);
    k.EnableTensorOffset();
    k.EnableTensorPitches();
    k.EnableBatching();
    k.EnableDifferentTypes();
    k.EnableDynamicShapesSupport();
    return k;
}

JitConstants GatherNonzeroKernelRef::GetJitConstants(const gather_nonzero_params& params) const {
    JitConstants jit = MakeBaseParamsJitConstants(params);
    const auto& input = params.inputs[0];
    jit.AddConstant(MakeJitConstant("OV_INPUT_RANK", params.ov_input_rank));
    auto max_local_mem_size = params.engineInfo.maxLocalMemSize / (params.outputs[0].ElementSize());
    if (input.is_dynamic()) {
        auto x = toCodeString(input.X(), 5);
        auto y = toCodeString(input.Y(), 4);
        auto z = toCodeString(input.Z(), 3);
        auto w = toCodeString(input.W(), 2);
        auto f = toCodeString(input.Feature(), 1);
        auto b = toCodeString(input.Batch(), 0);

        auto multiply = [](std::vector<std::string> dims) -> std::string {
            std::string res = "(";
            for (size_t i = 0; i < dims.size(); i++) {
                auto& d = dims[i];
                res += d;
                if (i != dims.size() - 1)
                    res += "*";
            }
            res += ")";
            return res;
        };
        const std::string total_data_size = multiply({x, y, z, w, f, b});
        jit.AddConstant(MakeJitConstant("TOTAL_DATA_SIZE", total_data_size));
        jit.AddConstant(MakeJitConstant("MAX_LOCAL_MEM_SIZE", max_local_mem_size));
    } else {
        jit.AddConstant(MakeJitConstant("TOTAL_DATA_SIZE", params.inputs[0].LogicalSize()));
        if (params.inputs[0].LogicalSize() * params.ov_input_rank < max_local_mem_size) {
            jit.AddConstant(MakeJitConstant("MAX_LOCAL_MEM_SIZE", max_local_mem_size));
            jit.AddConstant(MakeJitConstant("USE_LOCAL_MEM", 1));
        }
    }
    return jit;
}

CommonDispatchData GatherNonzeroKernelRef::SetDefault(const gather_nonzero_params& params) const {
    CommonDispatchData dispatchData;

    dispatchData.gws = {1, 1, 1};
    dispatchData.lws = {1, 1, 1};

    return dispatchData;
}

KernelsData GatherNonzeroKernelRef::GetKernelsData(const Params& params, const optional_params& options) const {
    assert(params.GetType() == KernelType::GATHER_NONZERO);

    KernelData kd = KernelData::Default<gather_nonzero_params>(params);
    gather_nonzero_params& newParams = *static_cast<gather_nonzero_params*>(kd.params.get());

    auto dispatchData = SetDefault(newParams);
    auto entry_point = GetEntryPoint(kernelName, newParams.layerID, params, options);
    auto cldnn_jit = GetJitConstants(newParams);
    auto jit = CreateJit(kernelName, cldnn_jit, entry_point);

    auto& kernel = kd.kernels[0];

    kd.update_dispatch_data_func = [this](const Params& params, KernelData& kd) {
        const auto& prim_params = static_cast<const gather_nonzero_params&>(params);
        auto dispatchData = SetDefault(prim_params);
        OPENVINO_ASSERT(kd.kernels.size() == 1, "[GPU] Invalid kernels size for update dispatch data func");
        kd.kernels[0].params.workGroups.global = dispatchData.gws;
        kd.kernels[0].params.workGroups.local = dispatchData.lws;
    };

    FillCLKernelData(kernel,
                     dispatchData,
                     params.engineInfo,
                     kernelName,
                     jit,
                     entry_point,
                     "",
                     false,
                     false,
                     2,
                     GetFusedPrimitiveInputsCount(params),
                     1,
                     newParams.outputs[0].is_dynamic());

    return {kd};
}

KernelsPriority GatherNonzeroKernelRef::GetKernelsPriority(const Params& /*params*/, const optional_params& /*options*/) const {
    return DONT_USE_IF_HAVE_SOMETHING_ELSE;
}

bool GatherNonzeroKernelRef::Validate(const Params& p, const optional_params& op) const {
    if (!KernelBaseOpenCL::Validate(p, op))
        return false;

    const auto& rp = static_cast<const gather_nonzero_params&>(p);

    return Tensor::SimpleLayout(rp.inputs[0].GetLayout());
}
}  // namespace kernel_selector
