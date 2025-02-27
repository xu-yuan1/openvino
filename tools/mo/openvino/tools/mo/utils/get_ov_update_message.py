# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import datetime

msg_fmt = 'Check for a new version of Intel(R) Distribution of OpenVINO(TM) toolkit here {0} ' \
          'or on https://github.com/openvinotoolkit/openvino'


def get_ov_update_message():
    expected_update_date = datetime.date(year=2023, month=5, day=1)
    current_date = datetime.date.today()

    link = 'https://software.intel.com/content/www/us/en/develop/tools/openvino-toolkit/download.html?cid=other&source=prod&campid=ww_2023_bu_IOTG_OpenVINO-2022-3&content=upg_all&medium=organic'

    return msg_fmt.format(link) if current_date >= expected_update_date else None


def get_ov_api20_message():
    link = "https://docs.openvino.ai/latest/openvino_2_0_transition_guide.html"
    message = '[ INFO ] The model was converted to IR v11, the latest model format that corresponds to the source DL framework ' \
              'input/output format. While IR v11 is backwards compatible with OpenVINO Inference Engine API v1.0, ' \
              'please use API v2.0 (as of 2022.1) to take advantage of the latest improvements in IR v11.\n' \
              'Find more information about API v2.0 and IR v11 at {}'.format(link)

    return message


def get_tf_fe_message():
    link = "https://docs.openvino.ai/latest/openvino_docs_MO_DG_TensorFlow_Frontend.html"
    message = '[ INFO ] IR generated by new TensorFlow Frontend is compatible only with API v2.0. Please make sure to use API v2.0.\n' \
              'Find more information about new TensorFlow Frontend at {}'.format(link)

    return message
