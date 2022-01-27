// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "detection_output_kernel_selector.h"
#include "detection_output_kernel_ref.h"

namespace kernel_selector {
detection_output_kernel_selector::detection_output_kernel_selector() { Attach<DetectionOutputKernelRef>(); }

KernelsData detection_output_kernel_selector::GetBestKernels(const Params& params, const optional_params& options) const {
    return GetNaiveBestKernel(params, options, KernelType::DETECTION_OUTPUT);
}
}  // namespace kernel_selector
