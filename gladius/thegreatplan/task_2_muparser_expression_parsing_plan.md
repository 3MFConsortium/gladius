# Plan: Mathematical Expression Parsing with muParser

**Date:** July 23, 2025

**Author:** GitHub Copilot

## 1. Overview

This document outlines the plan to integrate a mathematical expression parser into Gladius. The goal is to provide users with the ability to define components of a 3D model using mathematical formulas, which will then be converted into the graph-based representation used by Gladius. The plan also includes the reverse process: converting a graph back into a mathematical expression for verification and editing. We will use the `muParser` library for this purpose, as it is a lightweight C++ library for mathematical expression evaluation with good performance characteristics.

## 2. Key Features

- A new dialog in the model editor for users to input mathematical expressions.
- Real-time validation of the expression.
- Conversion of the mathematical expression into a Gladius node graph.
- Conversion of a Gladius node graph back into a mathematical expression.
- Integration of the generated graph into the main model.

## 3. Recommended Library: muParser

- **Name**: muParser
- **Description**: A fast mathematical expression parser library for C++. It can parse and evaluate mathematical expressions efficiently, making it ideal for our use case of converting expressions to a graph.
- **VCPKG**: muParser is available via vcpkg, which simplifies its integration into the Gladius build system.
- **Documentation**: [muParser Official Site](https://beltoforion.de/en/muparser/)
- **Advantages**: Lightweight, no heavy dependencies like LLVM, good performance, and supports variables and functions.

## 4. Step-by-Step Implementation Plan

### Step 1: Integrate muParser into the Project

- **Task**: Add `muParser` as a dependency in the `vcpkg.json.in` file.
- **Reasoning**: This is the first step to make the library's features available in the Gladius codebase. Using vcpkg will handle the download, build, and linkage of the library.
- **Verification**: Create a small test case or a temporary main function to parse a simple expression like `"x + y"` to ensure the library is correctly integrated.
- **Status**: ✅ COMPLETED - Added to vcpkg.json.in and CMakeLists.txt

### Step 2: Develop the Expression Parser

- **Task**: Create a new class, `ExpressionParser`, responsible for handling the expression logic.
- **Details**:
    - This class will take the string from the UI dialog.
    - It will use `muParser` to parse the string and validate syntax.
    - It must implement robust error handling to catch and report syntax errors in the expression.
    - It should extract variables from the expression and allow evaluation with variable values.
- **Reasoning**: Encapsulating the parsing logic in a dedicated class follows the Single Responsibility Principle and makes the code cleaner and easier to test.
- **Status**: ✅ COMPLETED - Created ExpressionParser.h/.cpp with full muParser integration

### Step 3: Design and Implement the UI Dialog

- **Task**: Create a new dialog using ImGui that allows users to enter a mathematical expression.
- **Details**:
    - The dialog should contain a text input field.
    - It should have "Apply", "Cancel", and "Preview" buttons.
    - The dialog should be launched from a new button or menu item in the model editor's UI.
    - Real-time validation with error display.
- **Reasoning**: A dedicated UI is necessary for user interaction. ImGui is already used in the project (inferred from `imgui.ini`), so we should use it for consistency.

### Step 4: Convert the Expression to a Gladius Node Graph

- **Task**: Implement the logic to analyze the parsed expression and create a corresponding Gladius node graph.
- **Details**:
    - Since muParser doesn't provide an AST like SymEngine, we'll need to analyze the expression differently.
    - We can create a recursive descent parser or use expression analysis to identify operations.
    - Each operation type (e.g., addition, multiplication, functions) will be mapped to a specific node type in the Gladius graph.
    - Variables will be mapped to appropriate input nodes in the graph.
- **Reasoning**: This is the core of the feature, bridging the gap between a user-friendly mathematical expression and the application's internal data structure.

### Step 5: Convert Gladius Node Graph to Expression

- **Task**: Implement the logic to traverse a Gladius node graph and construct a mathematical expression string.
- **Details**:
    - This will be the reverse of Step 4 and will involve a recursive function that starts from an output node and walks up the graph's inputs.
    - Each Gladius node type (e.g., "Addition", "Input") will be mapped to a mathematical operator or variable.
    - For example, an "Addition" node will be converted into a "+" operation, with its inputs recursively converted.
    - only nodes with one output will be considered (expressions can only be closed form). If a graph cannot be represented by an expressin the dialog should show a warning.
- **Reasoning**: This enables round-trip testing to verify the correctness of both conversion processes. It also opens up the possibility of editing existing graphs using the mathematical expression dialog.

### Step 6: Integrate with the Document Model

- **Task**: Connect the newly created graph to the main `Document`.
- **Details**:
    - When the user clicks "Apply", the generated graph from the `ExpressionParser` will be added to the current `Document`.
    - This will likely involve creating a new resource (e.g., a `FunctionResource`) that encapsulates the graph.
    - The model state will be updated, and the changes will be reflected in the viewport.
- **Reasoning**: The generated graph needs to be part of the document to be saved, loaded, and rendered.

### Step 7: Testing

- **Task**: Write comprehensive unit and integration tests.
- **Details**:
    - **Unit Tests**: ✅ COMPLETED
        - Test the `ExpressionParser` with valid and invalid expressions.
        - Test variable extraction and expression evaluation.
    - **Graph Conversion Tests**:
        - Test the expression-to-graph conversion for a variety of expressions.
        - Test the graph-to-expression conversion.
    - **Round-trip Tests**:
        - Convert an expression to a graph, then convert it back to an expression and verify it is mathematically equivalent.
        - Create a graph manually, convert it to an expression, then back to a graph, and verify the graphs are identical.
    - **Integration Tests**:
        - Test the full workflow from entering an expression in the UI to seeing the result in the renderer.
- **Reasoning**: Thorough testing is crucial to ensure the feature is robust and reliable.

## 5. Timeline

- **Week 1**: Step 1 & 2 (Integration and Parser) - ✅ COMPLETED
- **Week 2**: Step 3 (UI Dialog)
- **Week 3**: Step 4 & 5 (Bi-directional Conversion)
- **Week 4**: Step 6 & 7 (Integration and Testing)

## 6. Advantages of muParser over SymEngine

- **Lightweight**: No heavy dependencies like LLVM or GMP
- **Fast**: Optimized for numerical evaluation
- **Simple Integration**: Easy to add to existing projects
- **Good Error Reporting**: Clear error messages for syntax errors
- **Variable Support**: Easy variable definition and evaluation

This plan provides a structured approach to implementing the desired feature. Each step is designed to be a logical progression, minimizing risks and ensuring a high-quality result.
