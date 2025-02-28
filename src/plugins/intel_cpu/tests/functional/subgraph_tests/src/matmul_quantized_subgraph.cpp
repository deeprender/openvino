// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "test_utils/cpu_test_utils.hpp"
#include "test_utils/fusing_test_utils.hpp"
#include "ngraph_functions/builders.hpp"
#include "common_test_utils/common_utils.hpp"

#include <algorithm>
#include <cassert>

using namespace ngraph;
using namespace InferenceEngine;
using namespace CPUTestUtils;

namespace SubgraphTestsDefinitions {

using ElementType = ov::element::Type_t;
using MatmulBrgemmInt8TestParams = std::tuple<SizeVector,        // input shape
                                              bool,              // true: FullyConnected false: Matmul
                                              ElementType,       // input u8/s8
                                              ElementType,       // output f32/u8/s8
                                              CPUSpecificParams  // brgemm/jit primitive implement type
                                              >;

// subgraph:
//   fq->MatMul/FullyConnected->[fq]
// can cover brgemm avx2:
//   (u8/s8 + s8)->f32
//   (u8/s8 + s8)->u8/s8
class MatmulBrgemmInt8Test : public testing::WithParamInterface<MatmulBrgemmInt8TestParams>, public CpuTestWithFusing,
                      virtual public LayerTestsUtils::LayerTestsCommon {
public:
    static std::string getTestCaseName(testing::TestParamInfo<MatmulBrgemmInt8TestParams> obj) {
        SizeVector supportedInputShapes;
        bool isFC;
        ElementType inType;
        ElementType outType;
        CPUSpecificParams cpuParams;
        std::tie(supportedInputShapes, isFC, inType, outType, cpuParams) = obj.param;

        std::ostringstream result;
        result << "IS=" << ov::test::utils::vec2str(supportedInputShapes) << "_";
        result << (isFC ? "FullyConnected" : "MatMul") << "_";
        result << "InputType=" << inType << "_";
        result << "OutputType=" << outType << "_";
        result << CPUTestsBase::getTestCaseName(cpuParams);

        return result.str();
    }

protected:
    bool isFC;
    std::string nameMatmul = "TestedMatmul";
    ElementType inType;
    ElementType outType;
    void SetUp() override {
        targetDevice = ov::test::utils::DEVICE_CPU;
        SizeVector inShapes;
        CPUSpecificParams cpuParams;
        std::tie(inShapes, isFC, inType, outType, cpuParams) = this->GetParam();
        std::tie(inFmts, outFmts, priority, selectedType) = cpuParams;
        const auto ngPrec = element::f32;
        ov::ParameterVector inputParams {std::make_shared<ov::op::v0::Parameter>(ngPrec, ov::Shape(inShapes))};

        std::shared_ptr<Node> fq1;
        std::shared_ptr<Node> matMul;
        std::shared_ptr<Node> nodeBeforeConv;
        selectedType = makeSelectedTypeStr(selectedType, ElementType::i8);
        if (inType == ElementType::u8)
            fq1 = ngraph::builder::makeFakeQuantize(inputParams[0], ngPrec, 256, {}, {0.0f}, {2.55f}, {0.0f}, {2.55f});
        else
            fq1 = ngraph::builder::makeFakeQuantize(inputParams[0], ngPrec, 256, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f});

        if (isFC) {
            ngraph::Shape weightShape = inShapes;
            std::swap(weightShape[0], weightShape[1]);
            auto weightsNode = ngraph::builder::makeConstant(ngPrec, weightShape, std::vector<float>{0.0f}, true);
            auto fq2 = ngraph::builder::makeFakeQuantize(weightsNode, ngPrec, 256, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f});
            auto fc = std::make_shared<ngraph::opset1::MatMul>(fq1, fq2, false, false);
            fc->get_rt_info() = getCPUInfo();
            fc->set_friendly_name(nameMatmul);
            auto biasWeightsNode = ngraph::builder::makeConstant(ngPrec, {}, std::vector<float>{0.0f}, true);
            matMul = std::make_shared<ngraph::opset1::Add>(fc, biasWeightsNode);
        } else {
            auto fq2 = ngraph::builder::makeFakeQuantize(inputParams[0], ngPrec, 256, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f});
            matMul = builder::makeMatMul(fq1, fq2, false, true);
            matMul->get_rt_info() = getCPUInfo();
            matMul->set_friendly_name(nameMatmul);
        }
        if (outType == ElementType::u8)
            nodeBeforeConv = ngraph::builder::makeFakeQuantize(matMul, ngPrec, 256, {}, {0.0f}, {2.55f}, {0.0f}, {2.55f});
        else if (outType == ElementType::i8)
            nodeBeforeConv = ngraph::builder::makeFakeQuantize(matMul, ngPrec, 256, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f});
        else
            nodeBeforeConv = matMul;

        // matmul->fq->matmul can cover x8*s8->x8 case
        auto filterWeightsShape = matMul->get_output_shape(0);
        auto filterWeightsNode = ngraph::builder::makeConstant(element::f32, filterWeightsShape, std::vector<float>{}, true);
        auto fq3 = ngraph::builder::makeFakeQuantize(filterWeightsNode, ngPrec, 256, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f});
        // only matmul avx2 support s8*s8 input
        auto matMul2 = builder::makeMatMul(nodeBeforeConv, fq3, false, false);

        function = makeNgraphFunction(ngPrec, inputParams, matMul2, "MatmulBrgemmInt8");
    }

    void CheckNode(std::shared_ptr<const ov::Model> function, const std::string& nodeName) {
        ASSERT_NE(nullptr, function);
        for (const auto &node : function->get_ops()) {
            const auto & rtInfo = node->get_rt_info();
            auto getExecValue = [&rtInfo](const std::string & paramName) -> std::string {
                auto it = rtInfo.find(paramName);
                IE_ASSERT(rtInfo.end() != it);
                return it->second.as<std::string>();
            };
            if (node->get_friendly_name() == nodeName) {
                auto primType = getExecValue(ExecGraphInfoSerialization::IMPL_TYPE);
                ASSERT_TRUE(primTypeCheck(primType)) << "primType is unexpected: " << primType << " Expected: " << selectedType;
                ASSERT_EQ(node->get_output_element_type(0), outType);
                ASSERT_EQ(node->get_input_element_type(0), inType);
            }
        }
    }
};

TEST_P(MatmulBrgemmInt8Test, CompareWithRefs) {
    // only cover avx2_vnni
    if (InferenceEngine::with_cpu_x86_avx512_core() || !InferenceEngine::with_cpu_x86_avx2_vnni())
        GTEST_SKIP();

    Run();
    InferenceEngine::CNNNetwork execGraphInfo = executableNetwork.GetExecGraphInfo();
    auto exec = execGraphInfo.getFunction();
    CheckNode(exec, nameMatmul);
}

namespace {

const std::vector<SizeVector> supportedInputShapes = {
    {16, 32},
    {17, 15},
};

const std::vector<CPUSpecificParams>matmulSpecificFilterParams = {
    {{}, {}, {"brgemm_avx2"}, "brgemm_avx2"},
    {{}, {}, {"jit_gemm"}, "jit_gemm"}
};

INSTANTIATE_TEST_SUITE_P(smoke_matmulBrgemmInt8, MatmulBrgemmInt8Test,
                         ::testing::Combine(::testing::ValuesIn(supportedInputShapes),
                                            ::testing::ValuesIn({true, false}),
                                            ::testing::ValuesIn({ElementType::u8, ElementType::i8}),
                                            ::testing::ValuesIn({ElementType::f32, ElementType::u8, ElementType::i8}),
                                            ::testing::ValuesIn(matmulSpecificFilterParams)),
                         MatmulBrgemmInt8Test::getTestCaseName);

} // namespace

} // namespace SubgraphTestsDefinitions
