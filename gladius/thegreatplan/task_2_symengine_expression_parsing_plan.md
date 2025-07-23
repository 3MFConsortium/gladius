# Plan: Mathematical Expression Parsing with SymEngine

**Date:** July 23, 2025

**Author:** GitHub Copilot

## 1. Overview

This document outlines the plan to integrate a mathematical expression parser into Gladius. The goal is to provide users with the ability to define components of a 3D model using mathematical formulas, which will then be converted into the graph-based representation used by Gladius. We will use the `SymEngine` library for this purpose, as it is a powerful C++ library for symbolic computation that can parse expressions and generate an Abstract Syntax Tree (AST).

## 2. Key Features

- A new dialog in the model editor for users to input mathematical expressions.
- Real-time validation of the expression.
- Conversion of the mathematical expression into a Gladius node graph.
- Integration of the generated graph into the main model.

## 3. Recommended Library: SymEngine

- **Name**: SymEngine
- **Description**: A fast symbolic manipulation library for C++. It can parse mathematical expressions and represent them as an AST, which is ideal for our use case of converting expressions to a graph.
- **VCPKG**: SymEngine is available via vcpkg, which will simplify its integration into the Gladius build system.
- **Documentation**: [SymEngine GitHub](https://github.com/symengine/symengine)

## 4. Step-by-Step Implementation Plan

### Step 1: Integrate SymEngine into the Project

- **Task**: Add `SymEngine` as a dependency in the `vcpkg.json` file.
- **Reasoning**: This is the first step to make the library's features available in the Gladius codebase. Using vcpkg will handle the download, build, and linkage of the library.
- **Verification**: Create a small test case or a temporary main function to parse a simple expression like `"x + y"` to ensure the library is correctly integrated.

### Step 2: Design and Implement the UI Dialog

- **Task**: Create a new dialog using ImGui that allows users to enter a mathematical expression.
- **Details**:
    - The dialog should contain a text input field.
    - It should have "Apply", "Cancel", and "Preview" buttons.
    - The dialog should be launched from a new button or menu item in the model editor's UI.
- **Reasoning**: A dedicated UI is necessary for user interaction. ImGui is already used in the project (inferred from `imgui.ini`), so we should use it for consistency.

### Step 3: Develop the Expression Parser

- **Task**: Create a new class, `ExpressionParser`, responsible for handling the expression logic.
- **Details**:
    - This class will take the string from the UI dialog.
    - It will use `SymEngine` to parse the string into an AST.
    - It must implement robust error handling to catch and report syntax errors in the expression.
- **Reasoning**: Encapsulating the parsing logic in a dedicated class follows the Single Responsibility Principle and makes the code cleaner and easier to test.

### Step 4: Convert the AST to a Gladius Node Graph

- **Task**: Implement the logic to traverse the `SymEngine` AST and create a corresponding Gladius node graph.
- **Details**:
    - This will involve a visitor pattern or a recursive function that walks the AST.
    - Each node type in the AST (e.g., `Add`, `Mul`, `Symbol`, `Integer`) will be mapped to a specific node type in the Gladius graph.
    - For example, an `Add` AST node will create an "Addition" node in Gladius, with its children in the AST becoming the inputs to the Gladius node.
    - We will need to define which variables are permissible (e.g., `x`, `y`, `z`) and map them to appropriate input nodes in the graph.
- **Reasoning**: This is the core of the feature, bridging the gap between a user-friendly mathematical expression and the application's internal data structure.

### Step 5: Integrate with the Document Model

- **Task**: Connect the newly created graph to the main `Document`.
- **Details**:
    - When the user clicks "Apply", the generated graph from the `ExpressionParser` will be added to the current `Document`.
    - This will likely involve creating a new resource (e.g., a `FunctionResource`) that encapsulates the graph.
    - The model state will be updated, and the changes will be reflected in the viewport.
- **Reasoning**: The generated graph needs to be part of the document to be saved, loaded, and rendered.

### Step 6: Testing

- **Task**: Write comprehensive unit and integration tests.
- **Details**:
    - **Unit Tests**:
        - Test the `ExpressionParser` with valid and invalid expressions.
        - Test the AST-to-graph conversion for a variety of expressions (simple, nested, different functions).
    - **Integration Tests**:
        - Test the full workflow from entering an expression in the UI to seeing the result in the renderer.
- **Reasoning**: Thorough testing is crucial to ensure the feature is robust and reliable.

## 5. Timeline

- **Week 1**: Step 1 & 2 (Integration and UI).
- **Week 2-3**: Step 3 & 4 (Parser and AST to Graph conversion).
- **Week 4**: Step 5 & 6 (Integration and Testing).

This plan provides a structured approach to implementing the desired feature. Each step is designed to be a logical progression, minimizing risks and ensuring a high-quality result.
