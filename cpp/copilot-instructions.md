# Copilot instructions for the C++ implementation for amp-cucumber-cpp-query

## Architecture Overview

**Core Foundation**: The library is a multi platform, compiler indepented component to be consumed as a dependency for other cucumber components.

- The directory structure shall not separate headers from sources.
- All sources shall be in /cpp/cucumber/query/ and subdirectories

## Build System Conventions

**CMake Presets**: Use CMake presets extensively. Key presets:

- `Host` - Host tooling/tests
- `Coverage` - Test coverage analysis

## Development Workflow

**Container development**:  Strongly recommended to use devcontainer - contains all cross-compilation toolchains and tools. Commands assume container environment.

**C++ Core Guidelines**: MUST follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) at all times. These guidelines take precedence when not in conflict with the project-specific rules below.

**Code Style and Formatting**: MUST follow the [amp-embedded-infra-lib style guide](https://github.com/philips-software/amp-embedded-infra-lib/blob/main/documents/modules/ROOT/pages/CodingStandard.adoc). Use clang-format for formatting; configured via .clang-format.
- Use Include What You Use (IWYU) principles for header includes. Only include headers that are directly needed by the source file. Avoid including headers in headers unless necessary to reduce compilation dependencies.
  - This applies to both source and header files. Each file should include only the headers it directly uses, and not rely on transitive includes from other headers.
- Standard library headers should be included using angle brackets (`<...>`), e.g., `#include <vector>`.
- Project headers should be included using double quotes (`"..."`), e.g., `#include "my_header.hpp"`.
- External dependencies should be included using double quotes (`"..."`), e.g., `#include "nlohmann/json.hpp"`.
- Special note for nlohmann/json. json_fwd.hpp should be used in headers to avoid unnecessary inclusion of the full json.hpp, and json.hpp should be used in source files where the full implementation is needed. Unless there is a specific reason to include json.hpp in a header, prefer json_fwd.hpp to minimize compilation dependencies. This could be when defining functions that take or return nlohmann::json by value or when using features that require the full implementation in a header context.
- Identifiers shall not be acronyms, but rather written in full. For example, use `calculateArea()` instead of `calcArea()`, and `initializeDatabase()` instead of `initDB()`. This promotes readability and clarity in the codebase. Unless the identifier becomes too long or unwieldy, in which case a well-known abbreviation may be acceptable. (`i`, `j` and `k` for index is acceptable, but `idx` is not).
- Always write const-correct code. Const variables, const parameters, const members, and const member functions should be used wherever applicable to indicate immutability and to enable better optimization by the compiler. This includes marking member functions as const when they do not modify the state of the object, and using const references for parameters that are not modified within the function.
- Static private members should be declared in an anonymous namespace in the source file rather than as static members of a class, to limit their scope to the translation unit and avoid potential issues with static initialization order across different translation units.

**Build Commands**:
```sh
# List available presets
cmake --list-presets

# Configure and build for target
cmake --preset=<preset>
cmake --build --preset=<preset>

# Run all tests (Coverage preset only)
ctest --preset Coverage --test-dir build/Coverage

# Run a single test by name
ctest --preset Coverage --test-dir build/Coverage -R <test_name>
```

**Pull Requests and Commits**: Follow Conventional Commit style from CONTRIBUTING.md when proposing pull request titles and when creating commit messages.

## Testing Strategy

**Micro Tests**: Unit tests alongside source code in test/ subdirectories.

**Integration Tests**: Test input provided by /testdata/src.

**Test Doubles**: Mock/stub/fake implementations in test_doubles/ directories.

## File Naming Conventions

**Source files**: CamelCase.cpp/.hpp
**CMake targets and Executables**: follow directory structure like: prepend all with `cucumber_query_lib` and follow directory structure like: cucumber_query_lib, cucumber_query_lib.test, cucumber_query_lib.subdirectory, cucumber_query_lib.subdirectory.test etc.

## General Coding Rules

- Code shall not contain comments unless they are absolutely necessary and the user specifically asks for them.
- Constructors with one parameter must be an explicit constructor.
- Exceptions shall only be used for exceptional conditions. Not as a control flow mechanism.
- Avoid protected members in classes except test classes; prefer private members with public/protected accessors if needed.
- Always use override keyword for overridden virtual functions and do not use final keyword.
- Never use asserts, use exceptions instead. Custom exceptions are preferred, inherited from the best matching base exception.
