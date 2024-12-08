# C++ Coding Guidelines

## General
- **Modern C++**: Use C++11 and later features.
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

## Testing
- **Unit Tests**: Use GTest/GMock, follow `[UnitOfWork_StateUnderTest_ExpectedBehavior]` naming.
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

By following these guidelines, you can ensure clean, efficient, and maintainable C++ code.