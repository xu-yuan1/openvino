# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME ov_gna_func_tests)

addIeTargetTest(
        NAME ${TARGET_NAME}
        ROOT ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDENCIES
            openvino_intel_gna_plugin
        LINK_LIBRARIES
            funcSharedTests
        ADD_CLANG_FORMAT
        LABELS
            GNA
)
