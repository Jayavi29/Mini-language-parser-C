# Mini Programming Language Parser & Interpreter

A command-line parser and interpreter implemented in C for a small
custom-defined programming language. The project demonstrates lexical
analysis, recursive-descent parsing, expression evaluation, symbol-table
management, syntax and semantic error detection, and error recovery.

## Overview

The language supports:

- Integer variable declarations
- Arithmetic expressions
- Variable references
- `print` statements
- Parenthesized expressions
- Addition, subtraction, multiplication, and division

The parser follows a defined Context-Free Grammar (CFG) and uses a
recursive-descent parsing approach.

## Features

- Lexical analysis and tokenization
- Keyword, identifier, number, operator, and punctuation recognition
- Recursive-descent parser based on the defined CFG
- Integer variable declarations and assignment
- Arithmetic expression evaluation
- Operator precedence for `+`, `-`, `*`, and `/`
- Parenthesized expressions
- Symbol table for variable storage
- Syntax error detection with line and column information
- Semantic error detection
- Detection of undeclared variables
- Detection of variable redeclaration
- Division-by-zero detection
- Panic-mode error recovery
- Token dump mode for inspecting lexical analysis

## Supported Language

### Variable Declaration

```text
int x = 10;
int y = x + 5;