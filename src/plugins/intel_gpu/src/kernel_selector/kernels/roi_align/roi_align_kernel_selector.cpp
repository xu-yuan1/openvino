// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//
#include "roi_align_kernel_selector.h"
#include "roi_align_kernel_ref.h"

namespace kernel_selector {

roi_align_kernel_selector::roi_align_kernel_selector() {
    Attach<ROIAlignKernelRef>();
}

KernelsData roi_align_kernel_selector::GetBestKernels(const Params &params,
                                                      const optional_params &options) const {
    return GetNaiveBestKernel(params, options, KernelType::ROI_ALIGN);
}

} // namespace kernel_selector
