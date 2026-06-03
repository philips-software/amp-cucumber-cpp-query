# Copilot Instructions for amp-cucumber-cpp-query

This repository contains the **Query** component for the Cucumber framework — a polyglot library (C++ and JavaScript/TypeScript) that provides lookup functions over a stream of [Cucumber Messages](https://github.com/cucumber/messages).

## Repository Structure

| Folder | Purpose |
|--------|---------|
| `cpp/` | C++ implementation (C++20, CMake + CPM) |
| `javascript/` | TypeScript/JavaScript implementation (npm, Mocha) |
| `testdata/src/` | Shared NDJSON fixtures and expected `.results.json` files for acceptance tests |

## Cross-language Conventions

- Changes to query methods **must be implemented consistently across all languages** (see [CONTRIBUTING.md](../CONTRIBUTING.md)).
- Test fixtures in `testdata/src/` are generated from the Java reference implementation. Do not hand-edit `.results.json` files.
- Commit messages must follow [Conventional Commits](https://www.conventionalcommits.org/).

## Language-specific Instructions

Each language folder provides its own `copilot-instructions.md` with build commands, code style, and testing strategy:

- **C++**: [cpp/copilot-instructions.md](../cpp/copilot-instructions.md)
- **JavaScript/TypeScript**: not available yet

**Important**: At the start of every session, read the language-specific instructions file(s) relevant to the work being done and apply all rules from them unconditionally — do not wait to be asked.
