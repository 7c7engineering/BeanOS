# Style Guide

## Code Formatting

This project enforces **automatic code formatting** using [`clang-format`](https://clang.llvm.org/docs/ClangFormat.html).

* The formatting rules are defined in the `.clang-format` file at the repository root.
* **CI Enforcement**: A GitHub Actions pipeline automatically checks code formatting using `clang-format` version **18**.

  ```yaml
  - uses: actions/checkout@v4
  - name: Run clang-format style check for C/C++/Protobuf programs.
    uses: jidicula/clang-format-action@v4.15.0
    with:
      clang-format-version: '18'
      check-path: './'
  ```
* To format code locally, run the provided script:

  ```bash
  ./format.sh
  ```

  This script applies formatting to the entire source tree.

> **Important:** All code must be formatted before submitting a pull request. The CI pipeline will fail if formatting issues are found.

_Describe coding conventions, formatting rules, and best practices for this project._

- Naming conventions
- File organization
- Documentation standards
