# Axenlang Compiler
Axenc is a simple compiler for the Axen programming language written in c++ with llvm.

> [!NOTE]
> Axenc and the axenlang specifacaitons are both in early development.
> Please open issues if you have any.

## Installation
Axenc can be installed via the releases tab or from the [aur package](https://aur.archlinux.org/packages/axenc-git).

### Dependencies
- llvm 21

## Usage
```bash
# With object file output.
axenc -f path/to/root.ax -o path/to/output.o

# With llvm ir output (printed to stdout, for testing and debugging)
axenc -f path/to/root.ax
```

## License
This project is licensed under the **GNU General Public License v3.0** (GPL-3.0).

You are free to:
- Use, copy, modify, and distribute this software.
- Share your modifications under the same license.

See the full [LICENSE](LICENSE) file for full details.

## Contributions
Feel free to contribute (and help improve my messy C++).

Guidelines:
- Use imperative tense for commit messages.
- Run clang-format on any files you add or modify.
- Open issues for bugs or feature requests before submitting major changes.
- Fork the repository, create a branch for your changes, and submit a pull request.

Thank you for helping make this project better!
