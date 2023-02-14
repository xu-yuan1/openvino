// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ie_common.h>
#include <node.h>
#include <string>

namespace ov {
namespace intel_cpu {
namespace node {

class Pad : public Node {
public:
    Pad(const std::shared_ptr<ngraph::Node>& op, const GraphContext::CPtr context);

    static bool isSupportedOperation(const std::shared_ptr<const ngraph::Node>& op, std::string& errorMessage) noexcept;
    void getSupportedDescriptors() override;
    void initSupportedPrimitiveDescriptors() override;
    void createPrimitive() override;
    void execute(dnnl::stream strm) override;
    bool created() const override;

    void prepareParams() override;
    bool needShapeInfer() const override;
    bool isExecutable() const override;
    bool needPrepareParams() const override;

protected:
    void executeDynamicImpl(dnnl::stream strm) override;

private:
    enum PadMode {
        CONSTANT = 0,
        EDGE = 1,
        REFLECT = 2,
        SYMMETRIC = 3
    };

    struct PadAttrs {
        PadMode padMode = CONSTANT;
        float padValue = 0.f;
        std::vector<unsigned int> padsBegin;
        std::vector<unsigned int> padsEnd;
        int beginPadIdx = 0;
        int endPadIdx = 0;
        InferenceEngine::Precision prc;
        bool constPadValue = false;
    } attrs;

    struct PadExecutor {
        PadExecutor(const PadAttrs& attrs,
                    const std::vector<MemoryCPtr>& srcMemory,
                    const std::vector<MemoryCPtr>& dstMemory,
                    const std::string& errorPrefix);
        void exec(MemoryPtr& srcMemPtr, MemoryPtr& dstMemPtr);
        ~PadExecutor() = default;

    private:
        void padConstant(MemoryPtr& srcMemPtr, MemoryPtr& dstMemPtr);
        template<typename T> void padConstantCommon(MemoryPtr& srcMemPtr, MemoryPtr& dstMemPtr);
        void padConstantZero(MemoryPtr& srcMemPtr, MemoryPtr& dstMemPtr);
        void padEdge(MemoryPtr& srcMemPtr, MemoryPtr& dstMemPtr);
        void padReflectOrSymmetric(MemoryPtr& srcMemPtr, MemoryPtr& dstMemPtr, const bool isSymmetric = false);
        void paramsInitialization(const PadAttrs& attrs,
                            const std::vector<MemoryCPtr>& srcMemory,
                            const std::vector<MemoryCPtr>& dstMemory);
        void workPartition();
        inline void getDstIdx(const VectorDims& indexes, size_t& dstIdx) const;

        struct PadContext {
            PadExecutor* executor;
            MemoryPtr srcMemPtr;
            MemoryPtr dstMemPtr;
        };

        template<typename T>
        struct PadConstantEmitter {
            void operator()(PadContext& ctx) {
                ctx.executor->padConstantCommon<T>(ctx.srcMemPtr, ctx.dstMemPtr);
            }
        };

        bool zeroInputDimsCase = false;

        struct {
            PadAttrs attrs;
            VectorDims srcDims;
            VectorDims dstDims;
            VectorDims srcODims;
            VectorDims srcStrides;
            VectorDims dstStrides;
            VectorDims srcDimsForReflectOrSymmetric;
            int nThreads = 0;
            size_t nDimsForWork = 0lu;
            size_t workAmount = 0lu;
            size_t lastDstDim = 1lu;
            size_t shift = 0lu;
            size_t dataSize = 1lu;
            PadMode padMode;
        } params;
        const std::string errorPrefix;
    };

    static constexpr size_t DATA_ID = 0lu;
    static constexpr size_t PADS_BEGIN_ID = 1lu;
    static constexpr size_t PADS_END_ID = 2lu;
    static constexpr size_t PAD_VALUE_ID = 3lu;

    bool isPadValueSpecified = false;

    using executorPtr = std::shared_ptr<PadExecutor>;
    executorPtr execPtr = nullptr;
    std::vector<MemoryCPtr> srcMemory;
    std::vector<MemoryCPtr> dstMemory;
    std::string errorPrefix;
    bool shapeHasDataDependency = false;
};

}   // namespace node
}   // namespace intel_cpu
}   // namespace ov
