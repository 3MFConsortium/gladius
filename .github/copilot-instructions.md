# C++ Coding Guidelines

## General
- **Modern C++**: Use C++11 and later features. Use std::algorithm when possible.
- **Naming**: camelCase for variables/functions, PascalCase for classes/structs, UPPER_CASE for constants/macros.
- **Comments**: Use Doxygen-style comments.

## Code Structure
- **Headers**: Use `.h` for declarations, `.cpp` for definitions.
- **Include Guards**: Use `#pragma once`.
- **Namespaces**: Use `lower_snake_case`, avoid more than two levels.

## Types
- **Naming**: UpperCamelCase for all user-defined types.
- **Template Parameters**: Use descriptive names.

## Functions
- **Naming**: lowerCamelCase, start with a verb.
- **Boolean Functions**: Use `is/has/are` prefix.
- **Parameters**: Use `lowerCamelCase`.

## Variables
- **Naming**: lowerCamelCase, prefix with `m_` for private, `s_` for static, `g_` for global.
- **Constants**: Use UPPER_SNAKE_CASE.

## Memory Management
- **Smart Pointers**: Prefer `std::unique_ptr` and `std::shared_ptr`.

## Error Handling
- **Exceptions**: Use exceptions, avoid error codes.
- **Assertions**: Use `assert`.

## Performance
- **Inline Functions**: Use `inline` for small functions.
- **Const Correctness**: Use `const` wherever possible.
- **East-side const**: Place `const` on the right of the type being qualified (e.g., `int const*` rather than `const int*`).
- **Move Semantics**: Use move semantics for performance optimization.
- **Copy Elision**: Use copy elision to avoid unnecessary copies.
- **constexpr**: Use `constexpr` for compile-time constants.

## Testing
- **Unit Tests**: Use GTest/GMock.
- **Test Naming**: Follow `[UnitOfWork_StateUnderTest_ExpectedBehavior]` naming convention:
  - **UnitOfWork**: The method or function being tested
  - **StateUnderTest**: The inputs or conditions being tested
  - **ExpectedBehavior**: The expected outcome
  - Example: `RenderProgram_WithNullBuffer_ThrowsException`, `MeshResource_AfterLoading_ContainsCorrectVertexCount`
- **Test Implementation**: Each test should follow Arrange-Act-Assert pattern.
- **Namespaces**: Place tests in `namespace::tests`.

## Code Layout
- **Braces**: Use Allman style (opening brace on a new line).
- **Indentation**: Use 4 spaces, no tabs.
- **Line Length**: Max 160 characters, break lines as needed.

## Includes
- **Order**: Precompiled headers, project headers, external headers.
- **Syntax**: Use `""` for relative paths, `<>` for system paths.

## Best Practices
- **STL Containers**: Use `empty()` instead of `size()` to check for emptiness.
- **Fallthrough**: Use `[[fallthrough]]` only when necessary.

## Comments
- **Doxygen**: Use Doxygen comments for public APIs. Use `///` for single-line comments and `/** */` for multi-line comments.
- **TODOs**: Use `// TODO: description` for tasks to be completed later.
- **FIXME**: Use `// FIXME: description` for known issues that need fixing.
- **Documentatioon**: Only add comments that add additional value. Avoid stating the obvious.

By following these guidelines, you can ensure clean, efficient, and maintainable C++ code.