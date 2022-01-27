// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "mkldnn_matrix_nms_node.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "ie_parallel.hpp"
#include "ngraph/opsets/opset8.hpp"
#include "utils/general_utils.h"

using namespace MKLDNNPlugin;
using namespace InferenceEngine;

using ngNmsSortResultType = ngraph::op::util::NmsBase::SortResultType;
using ngNmseDcayFunction = ngraph::op::v8::MatrixNms::DecayFunction;

bool MKLDNNMatrixNmsNode::isSupportedOperation(const std::shared_ptr<const ngraph::Node>& op, std::string& errorMessage) noexcept {
    try {
        const auto nms = std::dynamic_pointer_cast<const ngraph::op::v8::MatrixNms>(op);
        if (!nms) {
            errorMessage = "Only MatrixNms operation is supported";
            return false;
        }
        const auto& attrs = nms->get_attrs();
        const auto& sortType = attrs.sort_result_type;
        if (!one_of(sortType, ngNmsSortResultType::NONE, ngNmsSortResultType::SCORE, ngNmsSortResultType::CLASSID)) {
            errorMessage = "Does not support SortResultType mode: " + ngraph::as_string(sortType);
            return false;
        }
        const auto& decayType = attrs.decay_function;
        if (!one_of(decayType, ngNmseDcayFunction::LINEAR, ngNmseDcayFunction::GAUSSIAN)) {
            errorMessage = "Does not support DcayFunction " + ngraph::as_string(decayType);
            return false;
        }
    } catch (...) {
        return false;
    }
    return true;
}

MKLDNNMatrixNmsNode::MKLDNNMatrixNmsNode(const std::shared_ptr<ngraph::Node>& op, const mkldnn::engine& eng, MKLDNNWeightsSharing::Ptr& cache)
    : MKLDNNNode(op, eng, cache) {
    std::string errorMessage;
    if (!isSupportedOperation(op, errorMessage)) {
        IE_THROW(NotImplemented) << errorMessage;
    }

    m_errorPrefix = "MatrixNMS layer with name '" + getName() + "' ";

    if (getOriginalInputsNumber() != 2)
        IE_THROW() << m_errorPrefix << "has incorrect number of input edges: " << getOriginalInputsNumber();

    if (getOriginalOutputsNumber() != 3)
        IE_THROW() << m_errorPrefix << "has incorrect number of output edges: " << getOriginalOutputsNumber();

    const auto matrix_nms = std::dynamic_pointer_cast<const ngraph::op::v8::MatrixNms>(op);

    auto& attrs = matrix_nms->get_attrs();
    if (attrs.sort_result_type == ngraph::op::util::NmsBase::SortResultType::CLASSID)
        m_sortResultType = MatrixNmsSortResultType::CLASSID;
    else if (attrs.sort_result_type == ngraph::op::util::NmsBase::SortResultType::SCORE)
        m_sortResultType = MatrixNmsSortResultType::SCORE;
    else if (attrs.sort_result_type == ngraph::op::util::NmsBase::SortResultType::NONE)
        m_sortResultType = MatrixNmsSortResultType::NONE;

    if (attrs.decay_function == ngraph::op::v8::MatrixNms::DecayFunction::GAUSSIAN)
        m_decayFunction = GAUSSIAN;
    else if (attrs.decay_function == ngraph::op::v8::MatrixNms::DecayFunction::LINEAR)
        m_decayFunction = LINEAR;

    m_sortResultAcrossBatch = attrs.sort_result_across_batch;
    m_scoreThreshold = attrs.score_threshold;
    m_nmsTopk = attrs.nms_top_k;
    m_keepTopk = attrs.keep_top_k;
    m_backgroundClass = attrs.background_class;

    m_gaussianSigma = attrs.gaussian_sigma;
    m_postThreshold = attrs.post_threshold;
    m_normalized = attrs.normalized;
    if (m_decayFunction == MatrixNmsDecayFunction::LINEAR) {
        m_decay_fn = [](float iou, float max_iou, float sigma) -> float {
            return (1. - iou) / (1. - max_iou + 1e-10f);
        };
    } else {
        m_decay_fn = [](float iou, float max_iou, float sigma) -> float {
            return std::exp((max_iou * max_iou - iou * iou) * sigma);
        };
    }

    const auto& boxes_dims = getInputShapeAtPort(NMS_BOXES).getDims();
    if (boxes_dims.size() != 3)
        IE_THROW() << m_errorPrefix << "has unsupported 'boxes' input rank: " << boxes_dims.size();
    if (boxes_dims[2] != 4)
        IE_THROW() << m_errorPrefix << "has unsupported 'boxes' input 3rd dimension size: " << boxes_dims[2];
    const auto& scores_dims = getInputShapeAtPort(NMS_SCORES).getDims();
    if (scores_dims.size() != 3)
        IE_THROW() << m_errorPrefix << "has unsupported 'scores' input rank: " << scores_dims.size();
}

void MKLDNNMatrixNmsNode::initSupportedPrimitiveDescriptors() {
    if (!supportedPrimitiveDescriptors.empty())
        return;

    const std::vector<Precision> supportedFloatPrecision = {Precision::FP32};
    const std::vector<Precision> supportedIntOutputPrecision = {Precision::I32, Precision::I64};

    checkPrecision(getOriginalInputPrecisionAtPort(NMS_BOXES), supportedFloatPrecision, "boxes", m_inType);
    checkPrecision(getOriginalInputPrecisionAtPort(NMS_SCORES), supportedFloatPrecision, "scores", m_inType);

    checkPrecision(getOriginalOutputPrecisionAtPort(NMS_SELECTED_INDICES), supportedIntOutputPrecision, "selected_indices", m_outType);
    checkPrecision(getOriginalOutputPrecisionAtPort(NMS_SELECTED_OUTPUTS), supportedFloatPrecision, "selected_outputs", m_outType);
    checkPrecision(getOriginalOutputPrecisionAtPort(NMS_VALID_OUTPUTS), supportedIntOutputPrecision, "valid_outputs", m_outType);

    addSupportedPrimDesc({{LayoutType::ncsp, Precision::FP32},
                          {LayoutType::ncsp, Precision::FP32}},
                         {{LayoutType::ncsp, Precision::FP32},
                          {LayoutType::ncsp, Precision::I32},
                          {LayoutType::ncsp, Precision::I32}},
                         impl_desc_type::ref_any);
}

bool MKLDNNMatrixNmsNode::created() const {
    return getType() == MatrixNms;
}

namespace {

static inline float boxArea(const float* bbox, const bool normalized) {
    if (bbox[2] < bbox[0] || bbox[3] < bbox[1]) {
        return static_cast<float>(0.);
    } else {
        const float width = bbox[2] - bbox[0];
        const float height = bbox[3] - bbox[1];
        if (normalized) {
            return width * height;
        } else {
            return (width + 1) * (height + 1);
        }
    }
}

static inline float intersectionOverUnion(const float* bbox1, const float* bbox2, const bool normalized) {
    if (bbox2[0] > bbox1[2] || bbox2[2] < bbox1[0] || bbox2[1] > bbox1[3] || bbox2[3] < bbox1[1]) {
        return static_cast<float>(0.);
    } else {
        const float xMin = std::max(bbox1[0], bbox2[0]);
        const float yMin = std::max(bbox1[1], bbox2[1]);
        const float xMax = std::min(bbox1[2], bbox2[2]);
        const float yMax = std::min(bbox1[3], bbox2[3]);
        float norm = normalized ? static_cast<float>(0.) : static_cast<float>(1.);
        float width = xMax - xMin + norm;
        float height = yMax - yMin + norm;
        const float interArea = width * height;
        const float bbox1Area = boxArea(bbox1, normalized);
        const float bbox2Area = boxArea(bbox2, normalized);
        return interArea / (bbox1Area + bbox2Area - interArea);
    }
}
}  // namespace

size_t MKLDNNMatrixNmsNode::nmsMatrix(const float* boxesData, const float* scoresData, BoxInfo* filterBoxes, const int64_t batchIdx, const int64_t classIdx) {
    std::vector<int32_t> candidateIndex(m_numBoxes);
    std::iota(candidateIndex.begin(), candidateIndex.end(), 0);
    auto end = std::remove_if(candidateIndex.begin(), candidateIndex.end(), [&scoresData, this](int32_t idx) {
        return scoresData[idx] <= m_scoreThreshold;
    });
    int64_t numDet = 0;
    int64_t originalSize = std::distance(candidateIndex.begin(), end);
    if (originalSize <= 0) {
        return 0;
    }
    if (m_nmsTopk > -1 && originalSize > m_nmsTopk) {
        originalSize = m_nmsTopk;
    }

    std::partial_sort(candidateIndex.begin(), candidateIndex.begin() + originalSize, end, [&scoresData](int32_t a, int32_t b) {
        return scoresData[a] > scoresData[b];
    });

    std::vector<float> iouMatrix((originalSize * (originalSize - 1)) >> 1);
    std::vector<float> iouMax(originalSize);

    iouMax[0] = 0.;
    InferenceEngine::parallel_for(originalSize - 1, [&](size_t i) {
        float max_iou = 0.;
        size_t actual_index = i + 1;
        auto idx_a = candidateIndex[actual_index];
        for (int64_t j = 0; j < actual_index; j++) {
            auto idx_b = candidateIndex[j];
            auto iou = intersectionOverUnion(boxesData + idx_a * 4, boxesData + idx_b * 4, m_normalized);
            max_iou = std::max(max_iou, iou);
            iouMatrix[actual_index * (actual_index - 1) / 2 + j] = iou;
        }
        iouMax[actual_index] = max_iou;
    });

    if (scoresData[candidateIndex[0]] > m_postThreshold) {
        auto box_index = candidateIndex[0];
        auto box = boxesData + box_index * 4;
        filterBoxes[0].box.x1 = box[0];
        filterBoxes[0].box.y1 = box[1];
        filterBoxes[0].box.x2 = box[2];
        filterBoxes[0].box.y2 = box[3];
        filterBoxes[0].index = batchIdx * m_numBoxes + box_index;
        filterBoxes[0].score = scoresData[candidateIndex[0]];
        filterBoxes[0].batchIndex = batchIdx;
        filterBoxes[0].classIndex = classIdx;
        numDet++;
    }

    for (int64_t i = 1; i < originalSize; i++) {
        float minDecay = 1.;
        for (int64_t j = 0; j < i; j++) {
            auto maxIou = iouMax[j];
            auto iou = iouMatrix[i * (i - 1) / 2 + j];
            auto decay = m_decay_fn(iou, maxIou, m_gaussianSigma);
            minDecay = std::min(minDecay, decay);
        }
        auto ds = minDecay * scoresData[candidateIndex[i]];
        if (ds <= m_postThreshold)
            continue;
        auto boxIndex = candidateIndex[i];
        auto box = boxesData + boxIndex * 4;
        filterBoxes[numDet].box.x1 = box[0];
        filterBoxes[numDet].box.y1 = box[1];
        filterBoxes[numDet].box.x2 = box[2];
        filterBoxes[numDet].box.y2 = box[3];
        filterBoxes[numDet].index = batchIdx * m_numBoxes + boxIndex;
        filterBoxes[numDet].score = ds;
        filterBoxes[numDet].batchIndex = batchIdx;
        filterBoxes[numDet].classIndex = classIdx;
        numDet++;
    }
    return numDet;
}

void MKLDNNMatrixNmsNode::prepareParams() {
    const auto& boxes_dims = getParentEdgeAt(NMS_BOXES)->getMemory().getStaticDims();
    const auto& scores_dims = getParentEdgeAt(NMS_SCORES)->getMemory().getStaticDims();
    if (!(boxes_dims[0] == scores_dims[0] && boxes_dims[1] == scores_dims[2])) {
        IE_THROW() << m_errorPrefix << "has incompatible 'boxes' and 'scores' input dmensions";
    }

    m_numBatches = boxes_dims[0];
    m_numBoxes = boxes_dims[1];

    m_numClasses = scores_dims[1];

    int64_t max_output_boxes_per_class = 0;
    size_t real_num_classes = m_backgroundClass == -1 ? m_numClasses :
        m_backgroundClass < m_numClasses ? m_numClasses - 1 : m_numClasses;
    if (m_nmsTopk >= 0)
        max_output_boxes_per_class = std::min(m_numBoxes, static_cast<size_t>(m_nmsTopk));
    else
        max_output_boxes_per_class = m_numBoxes;

    m_maxBoxesPerBatch = max_output_boxes_per_class * real_num_classes;
    if (m_keepTopk >= 0)
        m_maxBoxesPerBatch = std::min(m_maxBoxesPerBatch, static_cast<size_t>(m_keepTopk));

    m_realNumClasses = real_num_classes;
    m_realNumBoxes = m_nmsTopk == -1 ? m_numBoxes : std::min(m_nmsTopk, static_cast<int>(m_numBoxes));
    m_numPerBatch.resize(m_numBatches);
    m_filteredBoxes.resize(m_numBatches * m_realNumClasses * m_realNumBoxes);
    m_numPerBatchClass.resize(m_numBatches);
    for (auto &numPerBatch : m_numPerBatchClass) {
        numPerBatch.resize(m_numClasses, 0);
    }
    m_classOffset.resize(m_numClasses, 0);

    for (size_t i = 0, count = 0; i < m_numClasses; i++) {
        if (i == m_backgroundClass)
            continue;
        m_classOffset[i] = (count++) * m_realNumBoxes;
    }
}

bool MKLDNNMatrixNmsNode::isExecutable() const {
    return isDynamicNode() || MKLDNNNode::isExecutable();
}

void MKLDNNMatrixNmsNode::executeDynamicImpl(mkldnn::stream strm) {
    if (hasEmptyInputTensors()) {
        redefineOutputMemory({{0, 6}, {0, 1}, {0}});
        return;
    }
    execute(strm);
}

void MKLDNNMatrixNmsNode::execute(mkldnn::stream strm) {
    const float* boxes = reinterpret_cast<const float*>(getParentEdgeAt(NMS_BOXES)->getMemoryPtr()->GetPtr());
    const float* scores = reinterpret_cast<const float*>(getParentEdgeAt(NMS_SCORES)->getMemoryPtr()->GetPtr());

    InferenceEngine::parallel_for2d(m_numBatches, m_numClasses, [&](size_t batchIdx, size_t classIdx) {
        if (classIdx == m_backgroundClass) {
            m_numPerBatchClass[batchIdx][classIdx] = 0;
            return;
        }
        const float* boxesPtr = boxes + batchIdx * m_numBoxes * 4;
        const float* scoresPtr = scores + batchIdx * (m_numClasses * m_numBoxes) + classIdx * m_numBoxes;
        size_t classNumDet = 0;
        size_t batchOffset = batchIdx * m_realNumClasses * m_realNumBoxes;
        classNumDet = nmsMatrix(boxesPtr, scoresPtr, m_filteredBoxes.data() + batchOffset + m_classOffset[classIdx], batchIdx, classIdx);
        m_numPerBatchClass[batchIdx][classIdx] = classNumDet;
    });

    InferenceEngine::parallel_for(m_numBatches, [&](size_t batchIdx) {
        size_t batchOffset = batchIdx * m_realNumClasses * m_realNumBoxes;
        BoxInfo* batchFilteredBox = m_filteredBoxes.data() + batchOffset;
        auto& numPerClass = m_numPerBatchClass[batchIdx];
        auto numDet = std::accumulate(numPerClass.begin(), numPerClass.end(), 0);
        auto start_offset = numPerClass[0];

        for (size_t i = 1; i < numPerClass.size(); i++) {
            auto offset_class = m_classOffset[i];
            for (size_t j = 0; j < numPerClass[i]; j++) {
                batchFilteredBox[start_offset + j] = batchFilteredBox[offset_class + j];
            }
            start_offset += numPerClass[i];
        }
        auto keepNum = numDet;
        if (m_keepTopk > -1) {
            auto k = static_cast<size_t>(m_keepTopk);
            if (keepNum > k)
                keepNum = k;
        }

        std::partial_sort(batchFilteredBox, batchFilteredBox + keepNum, batchFilteredBox + numDet, [](const BoxInfo& lhs, const BoxInfo rhs) {
            return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.classIndex < rhs.classIndex) ||
                   (lhs.score == rhs.score && lhs.classIndex == rhs.classIndex && lhs.index < rhs.index);
        });
        m_numPerBatch[batchIdx] = keepNum;
    });

    auto startOffset = m_numPerBatch[0];
    for (size_t i = 1; i < m_numPerBatch.size(); i++) {
        auto offset_batch = i * m_realNumClasses * m_realNumBoxes;
        for (size_t j = 0; j < m_numPerBatch[i]; j++) {
            m_filteredBoxes[startOffset + j] = m_filteredBoxes[offset_batch + j];
        }
        startOffset += m_numPerBatch[i];
    }

    if (m_sortResultAcrossBatch) { /* sort across batch */
        if (m_sortResultType == MatrixNmsSortResultType::SCORE) {
            parallel_sort(m_filteredBoxes.begin(), m_filteredBoxes.begin() + startOffset, [](const BoxInfo& l, const BoxInfo& r) {
                return (l.score > r.score) || (l.score == r.score && l.batchIndex < r.batchIndex) ||
                       (l.score == r.score && l.batchIndex == r.batchIndex && l.classIndex < r.classIndex) ||
                       (l.score == r.score && l.batchIndex == r.batchIndex && l.classIndex == r.classIndex && l.index < r.index);
            });
        } else if (m_sortResultType == MatrixNmsSortResultType::CLASSID) {
            parallel_sort(m_filteredBoxes.begin(), m_filteredBoxes.begin() + startOffset, [](const BoxInfo& l, const BoxInfo& r) {
                return (l.classIndex < r.classIndex) || (l.classIndex == r.classIndex && l.batchIndex < r.batchIndex) ||
                       (l.classIndex == r.classIndex && l.batchIndex == r.batchIndex && l.score > r.score) ||
                       (l.classIndex == r.classIndex && l.batchIndex == r.batchIndex && l.score == r.score && l.index < r.index);
            });
        }
    }

    auto selectedOutputsMemPtr = getChildEdgesAtPort(NMS_SELECTED_OUTPUTS)[0]->getMemoryPtr();
    auto selectedIndicesMemPtr = getChildEdgesAtPort(NMS_SELECTED_INDICES)[0]->getMemoryPtr();
    auto validOutputsMemPtr = getChildEdgesAtPort(NMS_VALID_OUTPUTS)[0]->getMemoryPtr();

    // TODO [DS NMS]: remove when nodes from models where nms is not last node in model supports DS
    if (isDynamicNode()) {
        size_t totalBox = std::accumulate(m_numPerBatch.begin(), m_numPerBatch.end(), 0);
        redefineOutputMemory({{totalBox, 6}, {totalBox, 1}, {m_numBatches}});
    }
    float* selectedOutputs = reinterpret_cast<float*>(selectedOutputsMemPtr->GetPtr());
    int* selectedIndices = reinterpret_cast<int*>(selectedIndicesMemPtr->GetPtr());
    int* validOutputs = reinterpret_cast<int*>(validOutputsMemPtr->GetPtr());
    std::copy(m_numPerBatch.begin(), m_numPerBatch.end(), validOutputs);

    int64_t outputOffset = 0;
    int64_t originalOffset = 0;
    for (size_t i = 0; i < m_numBatches; i++) {
        auto real_boxes = m_numPerBatch[i];
        for (size_t j = 0; j < real_boxes; j++) {
            auto originalIndex = originalOffset + j;
            selectedIndices[j + outputOffset] = static_cast<int>(m_filteredBoxes[originalIndex].index);
            auto selectedBase = selectedOutputs + (outputOffset + j) * 6;
            selectedBase[0] = m_filteredBoxes[originalIndex].classIndex;
            selectedBase[1] = m_filteredBoxes[originalIndex].score;
            selectedBase[2] = m_filteredBoxes[originalIndex].box.x1;
            selectedBase[3] = m_filteredBoxes[originalIndex].box.y1;
            selectedBase[4] = m_filteredBoxes[originalIndex].box.x2;
            selectedBase[5] = m_filteredBoxes[originalIndex].box.y2;
        }
        // TODO [DS NMS]: remove when nodes from models where nms is not last node in model supports DS
        if (!isDynamicNode()) {
            std::fill_n(selectedOutputs + (outputOffset + real_boxes) * 6, (m_maxBoxesPerBatch - real_boxes) * 6, -1);
            std::fill_n(selectedIndices + (outputOffset + real_boxes), m_maxBoxesPerBatch - real_boxes, -1);
            outputOffset += m_maxBoxesPerBatch;
            originalOffset += real_boxes;
        } else {
            outputOffset += real_boxes;
            originalOffset += real_boxes;
        }
    }
}

void MKLDNNMatrixNmsNode::checkPrecision(const Precision prec, const std::vector<Precision> precList, const std::string name, const std::string type) {
    if (std::find(precList.begin(), precList.end(), prec) == precList.end())
        IE_THROW() << m_errorPrefix << "has unsupported '" << name << "' " << type << " precision: " << prec;
}

REG_MKLDNN_PRIM_FOR(MKLDNNMatrixNmsNode, MatrixNms);
