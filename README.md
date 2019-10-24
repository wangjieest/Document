# C++ and Document Compiler

## Goal

This is a command line tool for generating indexed code in HTML and documents from comments with Markdown for C++.

## "The compiler"

All **the compiler** here means the compiler created by this project.

## Requirements

- The compiler accepts a preprocessed cpp file from cl.exe.
  - The cpp file for preprocessed should compile using the same option which is also used to create the preprocessed file
- Comments containing documents should begin with `///`
- Comments should contain Markdown in XML tags
  - Details will be filled later

## Supported C++ Core Language Features

- **Supported**
  - [C++11](https://en.wikipedia.org/wiki/C%2B%2B11)
    - [x] Rvalue references and move constructors
    - [x] Uniform initialization
    - [x] Type inference
    - [x] Range-based for loop
    - [x] Alternative function syntax
    - [x] Null pointer constant
    - [x] Right angle bracket
    - [x] Template aliases
    - [x] Unrestricted unions
    - [x] Variadic templates
    - [x] New string literals
    - [x] Explicitly defaulted and deleted special member functions
  - [C++14](https://en.wikipedia.org/wiki/C%2B%2B14)
    - [x] Function return type deduction
    - [x] Alternate type deduction on declaration
    - [x] Variable templates
    - [x] Binary literals
  - [C++17](https://en.wikipedia.org/wiki/C%2B%2B17)
    - [x] Allow typename (as an alternative to class) in a template template parameter
    - [x] Nested namespace definitions, e.g., `namespace X::Y { … }` instead of namespace `X { namespace Y { … } }`
    - [x] Allowing attributes for namespaces and enumerators
    - [x] UTF-8 (u8) character literals (UTF-8 string literals have existed since C++11; C++17 adds the corresponding character literals for consistency, though as they are restricted to a single byte they can only store ASCII)
    - [x] Initializers in if and switch statements
  - [C++20](https://en.wikipedia.org/wiki/C%2B%2B20)
    - Review after publishing
- **Support before releasing**
  - [C++11](https://en.wikipedia.org/wiki/C%2B%2B11)
    - [ ] Initializer lists
      - (not supported yet)
    - [ ] Lambda functions and expressions
      - (not supported yet)
    - [ ] Object construction improvement
      - Constructor calls another constructor in initializing list (not supported yet)
      - Using base class' constructors (not supported yet)
    - [ ] Explicit overrides and final
      - `final` keyword (not supported yet)
    - [ ] Type long long int
      - (not supported yet)
    - [ ] Static assertions
      - (not supported yet)
    - [ ] Control and query object alignment
      - Need test
  - [C++14](https://en.wikipedia.org/wiki/C%2B%2B14)
    - [ ] Generic lambdas
      - (not supported yet)
    - [ ] Lambda capture expressions
      - (not supported yet)
  - [C++17](https://en.wikipedia.org/wiki/C%2B%2B17)
    - [ ] Making the text message for static_assert optional
      - (not supported yet)
    - [ ] New standard attributes `[[fallthrough]]`, `[[maybe_unused]]` and `[[nodiscard]]`
      - (not supported yet)
    - [ ] Fold expressions, for variadic templates
      - (not supported yet)
      - Need test
  - [C++20](https://en.wikipedia.org/wiki/C%2B%2B20)
    - Review after publishing
- **Parsed (if checked, or I will support it before eleasing) but it doesn't affect type inferencing so I don't care**
  - [C++11](https://en.wikipedia.org/wiki/C%2B%2B11)
    - [x] Thread-local storage
    - [x] Allow sizeof to work on members of classes without an explicit object:
    - [x] Attributes
    - [ ] Extern template
  - [C++14](https://en.wikipedia.org/wiki/C%2B%2B14)
    - [x] The attribute `[[deprecated]]`
  - [C++17](https://en.wikipedia.org/wiki/C%2B%2B17)
    - [x] copy-initialization and direct-initialization of objects of type `T` from prvalue expressions of type `T` (ignoring top-level cv-qualifiers) shall result in no copy or move constructors from the prvalue expression. See copy elision for more information. helper template function std::make_pair(5.0, false).
    - [x] Inline variables, which allows the definition of variables in header files without violating the one definition rule. The rules are effectively the same as inline functions
      - (not supported yet)
    - [x] Value of `__cplusplus` changed to 201703L: **Not Care: I don't do preprocessing by myself**
    - [ ] A compile-time static if with the form if constexpr(expression)
      - Need test
    - [ ] Structured binding declarations, allowing `auto [a, b] = getTwoReturnValues();`for   more information.
  - [C++20](https://en.wikipedia.org/wiki/C%2B%2B20)
    - Review after publishing
- **Parsed (if checked, or I will support it before eleasing) but it only reduces the accuracy of overloading so I don't care**
  - [C++11](https://en.wikipedia.org/wiki/C%2B%2B11)
    - [x] constexpr – Generalized constant expressions: **Treat all constant values identical**
    - [x] Modification to the definition of plain old data
    - [x] Strongly typed enumerations: **Type is ignored**
    - [x] Relaxed constexpr restrictions: **Treat all constant values identical**
    - [ ] Explicit conversion operators: **Treat all as implicit**
      - Need test to see if `explicit` is correctly parsed
  - [C++14](https://en.wikipedia.org/wiki/C%2B%2B14)
  - [C++17](https://en.wikipedia.org/wiki/C%2B%2B17)
    - [x] New rules for auto deduction from braced-init-list
    - [x] Constant evaluation for all non-type template arguments: **Treat all constant values identical**
    - [x] Exception specifications were made part of the function type: **Don't compare exception specifications**
  - [C++20](https://en.wikipedia.org/wiki/C%2B%2B20)
    - Review after publishing
- **No plan**
  - [C++11](https://en.wikipedia.org/wiki/C%2B%2B11)
    - [ ] User-defined literals
    - [ ] Multithreading memory model
    - [ ] Allow garbage collected implementations
  - [C++14](https://en.wikipedia.org/wiki/C%2B%2B14)
    - [ ] Aggregate member initialization
    - [ ] Digit separators
  - [C++17](https://en.wikipedia.org/wiki/C%2B%2B17)
    - [ ] Hexadecimal floating-point literals
    - [ ] Some extensions on over-aligned memory allocation
    - [ ] Class template argument deduction (CTAD), introducing constructor deduction guides, eg. allowing `std::pair(5.0, false)` instead of requiring explicit constructor arguments types `std::pair<double, bool>(5.0, false)` or an additional   helper template function std::make_pair(5.0, false).
    - [ ] `__has_include`, allowing the availability of a header to be checked by preprocessor directives
  - [C++20](https://en.wikipedia.org/wiki/C%2B%2B20)
    - Review after publishing

## Limitations

**All limitations could be changed in the future**.

### Syntax

- The compiler doesn't parse all C++11/C++14/C++17/C++20 features, but it parses the latest GacUI and most old C++ programs.
- All features that cl.exe doesn't support, are also not supported here.
- The compiler doesn't handle implicitly declared ctors or dtor for `union`, all `union` are assumed constructible in any default ways.
- The compiler ignores anonymous namespaces and `extern "C"` declarations, and parses sub declarations without creating an anonymous scope.
- The compiler doesn't recognize [user-defined literals](https://en.cppreference.com/w/cpp/language/user_literal), maybe I will finish it later.

### Semantic

- Candidates of names with overloading may not be completely narrowed because of incomplete template-related type inferencing.
  - The compiler doesn't do `const` or `constexpr` constant folding, the compiler treats all values identical.
    - This usually means sometimes multiple types will be returned due to overloadings or specializations.
    - This also means `enable_if` cannot narrow down candidates, all possible candidates will be returned.
- All members are treated as public.
- Implicitly generated members doesn't care if any base class is virtual.
- C style type reference is allowed to refer to a unexisting type. In this case, a forward declaration with the symbol is created in the root namespace.

### Analyzing Code

- The compiler parses all function bodies after the whole program is analyzed, so the behavior will be slightly different from the C++ standard.
  - One exception is that, if the type of the function is required, and the return type depends of the function body, then the body will be parsed at that moment.
- The compiler doesn't check bodies of template declarations when creating their instances.
  - For example, the compiler resolves the body of a template function right after it is parsed. All recognizable names will be identified at this moment. The body will not be processed again later when this function is instanciating in somewhere else.
  - The reason for this is, **GacUI** and its dependeices offers a lot of template declarations. They are supposed to be used by users. So there is no reason not to process template function bodies if they are not used inside the library. There is also no reason to offer more detailed indexing just for all places using these template functions inside the library.

## If you want to use it but you have trouble processing your own source code

- This project is not open sourced. I may change the license once the compiler is ready to release.
- I will announce when I think the compiler is ready to release. Now it is not completed so you are expected to have trouble.
- If you believe the compiler should be able to handle a certain valid case but it doesn't, please open an issue with a minimum reproduction, including your preprocessed file produced by cl.exe.
  - If the compiler cannot parse any Windows SDK file, I will consider it to be a bug and fix it.
  - Narrowing down candidates more precisely is not considered at this moment.
