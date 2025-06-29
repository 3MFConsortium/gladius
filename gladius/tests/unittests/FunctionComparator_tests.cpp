#include <gtest/gtest.h>
#include <iostream>

#include "io/3mf/FunctionComparator.h"
#include <lib3mf_implicit.hpp>

class FunctionComparator_Test : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_wrapper = Lib3MF::CWrapper::loadLibrary();
        m_model = m_wrapper->CreateModel();
    }

    virtual void TearDown()
    {
        m_wrapper.reset();
        m_model.reset();
    }

    Lib3MF::PWrapper m_wrapper;
    Lib3MF::PModel m_model;

    // Helper function to create a simple implicit function with an addition node
    Lib3MF::PImplicitFunction createSimpleFunction(const std::string& displayName)
    {
        Lib3MF::PImplicitFunction function = m_model->AddImplicitFunction();
        function->SetDisplayName(displayName);
        
        // Add input ports
        function->AddInput("pos", "position", Lib3MF::eImplicitPortType::Vector);
        
        // Add an addition node
        function->AddAdditionNode(
            "addition_1", Lib3MF::eImplicitNodeConfiguration::ScalarToScalar,
            "addition 1", "");
        
        // Add output
        function->AddOutput("shape", "output shape", Lib3MF::eImplicitPortType::Scalar);
        
        return function;
    }

    // Helper to create a function with multiple nodes and links
    Lib3MF::PImplicitFunction createComplexFunction(const std::string& displayName)
    {
        Lib3MF::PImplicitFunction function = m_model->AddImplicitFunction();
        function->SetDisplayName(displayName);
        
        // Add inputs
        auto functionArgument = function->AddInput("pos", "position", Lib3MF::eImplicitPortType::Vector);
        
        // Add nodes
        auto addNode = function->AddAdditionNode(
            "addition_1", Lib3MF::eImplicitNodeConfiguration::ScalarToScalar,
            "addition 1", "");
        
        auto subNode = function->AddSubtractionNode(
            "subtraction_1", Lib3MF::eImplicitNodeConfiguration::ScalarToScalar,
            "subtraction 1", "");
        
        auto mulNode = function->AddMultiplicationNode(
            "multiplication_1", Lib3MF::eImplicitNodeConfiguration::ScalarToScalar,
            "multiplication 1", "");
        
        // Add some links
        auto outputResult = addNode->GetOutputResult();
        auto inputA = subNode->GetInputA();
        function->AddLink(outputResult, inputA);
        
        // Alternative way to add links
        mulNode->FindInput("A")->SetReference("addition_1.result");
        
        // Add output
        auto output = function->AddOutput("shape", "signed distance to the surface", 
                                        Lib3MF::eImplicitPortType::Scalar);
        
        auto subNodeOutputResult = subNode->GetOutputResult();
        function->AddLink(subNodeOutputResult, output);
        
        return function;
    }
};

TEST_F(FunctionComparator_Test, SimpleFunction_Equal)
{
    // Create two identical simple functions
    auto function1 = createSimpleFunction("test");
    auto function2 = createSimpleFunction("test");
    
    // They should be equal
    EXPECT_TRUE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function1.get()),
        dynamic_cast<Lib3MF::CImplicitFunction*>(function2.get())));
}

TEST_F(FunctionComparator_Test, SimpleFunction_DifferentNames_NotEqual)
{
    // Create two functions with different names
    auto function1 = createSimpleFunction("test1");
    auto function2 = createSimpleFunction("test2");
    
    // They should not be equal
    EXPECT_FALSE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function1.get()),
        dynamic_cast<Lib3MF::CImplicitFunction*>(function2.get())));
}

TEST_F(FunctionComparator_Test, ComplexFunction_Equal)
{
    // Create two identical complex functions
    auto function1 = createComplexFunction("complex");
    auto function2 = createComplexFunction("complex");
    
    // They should be equal
    EXPECT_TRUE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function1.get()),
        dynamic_cast<Lib3MF::CImplicitFunction*>(function2.get())));
}

TEST_F(FunctionComparator_Test, ComplexFunction_DifferentNodes_NotEqual)
{
    // Create one complex function
    auto function1 = createComplexFunction("complex");
    
    // Create another function with a different structure
    auto function2 = m_model->AddImplicitFunction();
    function2->SetDisplayName("complex");
    function2->AddInput("pos", "position", Lib3MF::eImplicitPortType::Vector);
    
    // Add different nodes
    function2->AddSubtractionNode(
        "subtraction_1", Lib3MF::eImplicitNodeConfiguration::ScalarToScalar,
        "subtraction 1", "");
    
    function2->AddAdditionNode(
        "addition_1", Lib3MF::eImplicitNodeConfiguration::ScalarToScalar,
        "addition 1", "");
    
    function2->AddOutput("shape", "signed distance to the surface", 
                        Lib3MF::eImplicitPortType::Scalar);
    
    // They should not be equal due to different node structure
    EXPECT_FALSE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function1.get()),
        dynamic_cast<Lib3MF::CImplicitFunction*>(function2.get())));
}

TEST_F(FunctionComparator_Test, FunctionWithConstantNodes_Equal)
{
    // Create two functions with identical constant nodes
    auto function1 = m_model->AddImplicitFunction();
    function1->SetDisplayName("constants");
    
    auto function2 = m_model->AddImplicitFunction();
    function2->SetDisplayName("constants");
    
    // Add scalar constant nodes
    auto constNode1 = function1->AddConstantNode("const_1", "const value 1", "");
    constNode1->SetConstant(5.0);
    auto constNode2 = function2->AddConstantNode("const_1", "const value 1", "");
    constNode2->SetConstant(5.0);
    
    // Add vector constant nodes
    Lib3MF::sVector vec = {1.0, 2.0, 3.0};
    auto vecNode1 = function1->AddConstVecNode("vec_1", "const vector 1", "");
    vecNode1->SetVector(vec);
    auto vecNode2 = function2->AddConstVecNode("vec_1", "const vector 1", "");
    vecNode2->SetVector(vec);
    
    // They should be equal
    EXPECT_TRUE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function1.get()),
        dynamic_cast<Lib3MF::CImplicitFunction*>(function2.get())));
}

TEST_F(FunctionComparator_Test, FunctionWithConstantNodes_DifferentValues_NotEqual)
{
    // Create two functions with different constant values
    auto function1 = m_model->AddImplicitFunction();
    function1->SetDisplayName("constants");
    
    auto function2 = m_model->AddImplicitFunction();
    function2->SetDisplayName("constants");
    
    // Add scalar constant nodes with different values
    auto constNode1 = function1->AddConstantNode("const_1", "const value 1", "");
    constNode1->SetConstant(5.0);
    auto constNode2 = function2->AddConstantNode("const_1", "const value 1", "");
    constNode2->SetConstant(7.0);
    
    // They should not be equal due to different constant values
    EXPECT_FALSE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function1.get()),
        dynamic_cast<Lib3MF::CImplicitFunction*>(function2.get())));
}

TEST_F(FunctionComparator_Test, FunctionWithNullptr_NotEqual)
{
    // Create one function
    auto function = createSimpleFunction("test");
    
    // Compare with nullptr
    EXPECT_FALSE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function.get()),
        nullptr));
}

TEST_F(FunctionComparator_Test, BothFunctionsNullptr_Equal)
{
    // Two nullptr functions should be equal
    EXPECT_TRUE(gladius::io::areImplicitFunctionsEqual(nullptr, nullptr));
}

TEST_F(FunctionComparator_Test, FunctionsWithMatrixNodes_Equal)
{
    // Create two functions with identical matrix nodes
    auto function1 = m_model->AddImplicitFunction();
    function1->SetDisplayName("matrices");
    
    auto function2 = m_model->AddImplicitFunction();
    function2->SetDisplayName("matrices");
    
    // Create a matrix
    Lib3MF::sMatrix4x4 matrix;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            matrix.m_Field[i][j] = i + j * 0.1;
        }
    }
    
    // Add matrix nodes
    auto matNode1 = function1->AddConstMatNode("mat_1", "const matrix 1", "");
    matNode1->SetMatrix(matrix);
    auto matNode2 = function2->AddConstMatNode("mat_1", "const matrix 1", "");
    matNode2->SetMatrix(matrix);
    
    // They should be equal
    EXPECT_TRUE(gladius::io::areImplicitFunctionsEqual(
        dynamic_cast<Lib3MF::CImplicitFunction*>(function1.get()),
        dynamic_cast<Lib3MF::CImplicitFunction*>(function2.get())));
}
