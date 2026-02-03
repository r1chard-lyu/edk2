# EDK II (edk2) Copilot Development Instructions

This document provides comprehensive guidelines for AI-assisted development in the EDK II project, covering code style, commit practices, and development workflow.

---

## 1. C Code Style Guidelines

### Naming Conventions
- **CamelCase** for variables, functions, and file names
  - Correct: `FooFileName.h`, `SuperFunction()`, `ALocalVariable`
  - Incorrect: `foo-file_name.h`, `superFunction()`, `a_local_variable`

- **UPPERCASE** for types and macros
  - Correct: `#define FOO_MACRO(a,b)`, `typedef STRUCT_NAME`
  - Incorrect: `#define FooMacro(a,b)`, `typedef struct_name`

### UEFI Types (NOT C Types)
Use UEFI types instead of standard C types:
- `INTN` instead of `int`
- `UINTN` instead of `unsigned int`
- `VOID` instead of `void`
- `VOID*` instead of `void*`

### Formatting Requirements
- **Line length**: Maximum 80 characters
- **Indentation**: 2 spaces (never use tabs)
- **Braces**: Always use `{ }` even for single statements
  - Opening brace `{` goes at end of previous line
  - Function opening brace `{` goes on new line
- **Spacing**: Single space around operators, no space inside parentheses

**Example:**
```c
/**
  Brief description.
  
  @param[in]      Arg1 Description.
  @param[out]     Arg2 Description.
  
  @retval EFI_SUCCESS   Success.
  @retval !EFI_SUCCESS  Failure.
**/
EFI_STATUS
EFIAPI
FunctionName (
  IN     UINTN  Arg1,
  OUT    UINTN  *Arg2
  )
{
  UINTN Local;

  if (Arg1 > 0) {
    Print (L"Value: %d\n", Arg1);
  }

  return EFI_SUCCESS;
}
```

### Code Formatting Tool
- **Uncrustify** is REQUIRED for all C code formatting
- Run after each commit: `uncrustify -c edk2/.uncrustify -i <file>`
- For details: [EDK-II-Code-Formatting](https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Code-Formatting)
- Reference: [EDK II C Coding Standards 2.2](https://tianocore-docs.github.io/edk2-CCodingStandardsSpecification/release-2.20/)

---

## 2. Python Code Style (PEP 8)

### Indentation and Line Length
- **Indentation**: 4 spaces per level (never use tabs)
- **Line length**: Maximum 79 characters
- **Docstrings/comments**: Maximum 72 characters
- Use parentheses for line continuation, not backslash

### Naming Conventions
- `lowercase_with_underscores` for functions and variables
- `UPPERCASE_WITH_UNDERSCORES` for constants
- `CapWords` for classes
- Avoid single letter names except for loop counters
- Use descriptive names (avoid `a`, `b`, `x` unless obvious)

### Imports
- Group imports: standard library, third party, local application
- One blank line between groups
- Avoid wildcard imports (`from module import *`)
- Absolute imports preferred over relative imports

### Documentation
- Use docstrings for modules, functions, classes
- Use `#` for inline comments (sparingly)
- Comments should explain WHY, not WHAT

**Example:**
```python
def process_firmware(input_file, output_file):
  """Process firmware file and write output.
  
  Args:
    input_file: Path to input firmware file.
    output_file: Path to output file.
    
  Returns:
    True if successful, False otherwise.
  """
  data = read_binary_file(input_file)
  if data is None:
    return False
  
  result = transform_data(data)
  return write_binary_file(output_file, result)
```

Reference: [PEP 8 – Style Guide for Python Code](https://peps.python.org/pep-0008/)

---

## 3. Commit Partitioning

### Principles
- **One logical change per commit**: Fix one issue or add one feature
- **Each commit must build**: Ensures `git bisect` functionality
- **Each commit must function**: Code should be testable independently
- **Don't over-partition**: Don't create 5 commits for 5 spelling fixes in one file

### Examples of Separate Commits
- Library/PPI/Protocol interface separate from implementation
- Multiple distinct fixes in same file (if truly separate)

### Examples of Single Commits
- Single spelling pass through a file
- Complete new driver or library implementation
- Related changes in multiple files that form one logical unit

### Handling git bisect Issues
- Reorder commits if needed to keep each one building
- Add temporary code that is removed later if necessary
- Test with Microsoft and GCC toolchains if possible

Reference: [Commit-Partitioning](https://github.com/tianocore/tianocore.github.io/wiki/Commit-Partitioning)

---

## 4. Commit Message Format

### Structure
```
Pkg-Module: Brief-single-line-summary

Full-commit-message body describing the change
in detail, wrapped at less than 76 characters
when possible.

Signed-off-by: Contributor Name <contributor@email.server>
```

### Rules
- **Subject line**: `Pkg-Module: Brief-summary`
  - Total length `Pkg-Module: summary` < 72 characters
  - For CVE fixes: append `(CVE-YYYY-NNNN)`, total < 92 characters
  
- **Blank line**: Empty line between subject and body (no whitespace)

- **Body**: 
  - Describe the change in detail
  - Line length < 76 characters when possible
  - Explain WHY the change was made
  - Include test verification items

- **For multi-package commits**:
  - < 4 packages: `Package1,Package2,Package3/Module: Summary`
  - ≥ 4 packages: `Global: Summary`

- **Signatures**: Required (see Commit Signature Format below)

- **Character encoding**: ASCII only

### Example
```
MdePkg/Library: Fix uninitialized variable warning

The UINT32 variable Status was used without initialization
in error handling path. Initialize to EFI_INVALID_PARAMETER
to ensure proper error reporting.

Signed-off-by: John Developer <john.developer@example.com>
```

### CVE Example
```
SecurityPkg: Fix buffer overflow vulnerability (CVE-2024-12345)

Fix potential buffer overflow in TLS processing that could
allow remote code execution.

Signed-off-by: Jane Maintainer <jane.maintainer@example.com>
```

Reference: [Commit-Message-Format](https://github.com/tianocore/tianocore.github.io/wiki/Commit-Message-Format)

---

## 5. Commit Signature Format

### Signed-off-by Tag
- **Required** for all commits
- Indicates you are the author or know the code is usable
- Format: `Signed-off-by: Your Real Name <your.email@example.com>`
- Git shortcut: `git commit -s` auto-appends this

### Reviewed-by Tag (PR Process)
- Added by reviewers during pull request review
- Format: `Reviewed-by: Reviewer Name <reviewer@example.com>`
- Committer adds this before merging

### Placement Rules
- All signatures at end of commit message
- One blank line before signature block
- One signature per line
- Add new signatures to end of existing signature list
- Strict format for automated tools

### Name Format
- Use real name and email
- If name contains comma, quote it: `"Last, First" <email@example.com>`

### Example Full Commit
```
UefiCpuPkg/MpInitLib: Fix uninitialized variable warning

The GhcbApicIds variable was referenced without initialization
in certain code paths. Initialize array to zero to ensure
proper behavior in all scenarios.

Tested with:
- OVMF build with GCC 9.3
- QEMU SMP configuration

Signed-off-by: Alice Developer <alice.dev@example.com>
Reviewed-by: Bob Maintainer <bob.maint@example.com>
```

Reference: [Commit-Signature-Format](https://github.com/tianocore/tianocore.github.io/wiki/Commit-Signature-Format)

---

## 6. Pre-Commit Validation

### PatchCheck.py Validation
Run **before** pushing commits:
```bash
# Check latest N commits
python BaseTools/Scripts/PatchCheck.py -N

# Example: check latest 2 commits
python BaseTools/Scripts/PatchCheck.py -2
```

This validates:
- Commit message format compliance
- Line length requirements
- Proper signatures
- File naming conventions

### Code Formatting
Run **before** pushing commits:
```bash
# Check and fix formatting
uncrustify -c edk2/.uncrustify -i <filename>
```

### Local Build Testing
```bash
# Use Stuart to run CI checks locally
# Build code
python -m pip install -r pip-requirements.txt
python Basetools/Scripts/Stuart/build.py

# Run CI plugins for static analysis
python Basetools/Scripts/Stuart/ci_build.py
```

---

## 7. Pull Request Process

### Creating a PR
1. Fork the edk2 repository to your account
2. Create feature branch: `git checkout -b fix-issue origin/master`
3. Make changes and commit (following all guidelines above)
4. Rebase on latest master: `git rebase origin/master`
5. Push to your fork: `git push <your-fork> <branch>`
6. Create PR on GitHub with:
   - Descriptive title
   - Reference issue: `Fixes https://github.com/tianocore/edk2/issues/XXXX`
   - Add appropriate reviewers from `Maintainers.txt`

### PR Requirements
- All commits must be independent (no squashing)
- Pass PatchCheck.py validation
- Pass code formatting checks
- Pass local CI checks
- All conversations resolved

### Addressing Feedback
1. Make changes locally
2. Amend commits: `git commit --amend` (for latest)
3. Or use interactive rebase: `git rebase -i origin/master` (for multiple)
4. Force push to update PR: `git push -f <your-fork> <branch>`
5. Leave comment describing resolution

Reference: [EDK-II-Development-Process](https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Development-Process)

---

## 8. Development Workflow Summary

### For Every Code Change

1. **Create topic branch**
   ```bash
   git checkout -b <feature-branch> origin/master
   ```

2. **Make changes and commit**
   - Follow C/Python style guidelines
   - Partition logically
   - Write clear commit messages

3. **Run validation tools** (before each commit)
   ```bash
   python BaseTools/Scripts/PatchCheck.py -1
   uncrustify -c edk2/.uncrustify -i <files>
   ```

4. **Compile and test locally**
   ```bash
   python -m pip install -r pip-requirements.txt
   python Basetools/Scripts/Stuart/ci_build.py
   ```

5. **Rebase and push**
   ```bash
   git fetch origin
   git rebase origin/master
   git push <fork> <feature-branch>
   ```

6. **Create pull request**
   - Add issue link
   - Add appropriate reviewers

7. **Address feedback**
   - Update commits
   - Force push
   - Leave resolution comment

---

## 9. Tools and Resources

### Build and Validation Tools
- **Uncrustify**: Code formatter for C ([EDK-II-Code-Formatting](https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Code-Formatting))
- **PatchCheck.py**: Commit validation (`BaseTools/Scripts/`)
- **Stuart**: Build system and CI runner ([How-to-Build-With-Stuart](https://github.com/tianocore/tianocore.github.io/wiki/How-to-Build-With-Stuart))
- **GetMaintainer.py**: Find code area maintainers (`BaseTools/Scripts/`)

### Key Files
- `Maintainers.txt`: List of package/module maintainers
- `Contributions.txt`: Contribution guidelines in source tree
- `pip-requirements.txt`: Python dependencies

### Important References
- [EDK II Development Process](https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Development-Process)
- [Getting Started with EDK II](https://github.com/tianocore/tianocore.github.io/wiki/Getting-Started-with-EDK-II)
- [Build Instructions](https://github.com/tianocore/tianocore.github.io/wiki/Build-Instructions)
- [Code-Style](https://github.com/tianocore/tianocore.github.io/wiki/Code-Style)
- [Inclusive Language Guidelines](https://github.com/tianocore/tianocore.github.io/wiki/Inclusive-Language-Guidelines)

---

## 10. Common Mistakes to Avoid

❌ **DON'T:**
- Use C types (`int`, `void*`, `unsigned int`) instead of UEFI types
- Mix tabs and spaces for indentation
- Use snake_case for C variables/functions
- Write commits that don't compile or test
- Squash commits in a PR
- Forget Signed-off-by tags
- Use lines > 80 chars (C) or 79 chars (Python)
- Omit braces in C code (`if` statements)
- Create PRs without running PatchCheck.py
- Leave conversations unresolved in PRs

✅ **DO:**
- Use UEFI types (`UINTN`, `VOID*`, `INTN`)
- Use spaces for indentation (2 spaces for C, 4 for Python)
- Use CamelCase for C, snake_case for Python
- Ensure each commit builds independently
- Keep commits logically separated
- Always include Signed-off-by
- Run code formatter after each commit
- Always include braces, even for single statements
- Validate with PatchCheck.py before pushing
- Resolve all PR conversations before merge

---

## 11. Getting Help

If you need assistance:
- Create a [GitHub Discussion](https://github.com/tianocore/edk2/discussions) for questions
- Ask in pull request comments
- Consult git experts on edk2-devel mailing list or IRC
- Review existing code for style examples

---

**Last Updated**: 2024
**Based on**: EDK II Development Process, PEP 8, Code Style Guidelines
