# ExPResSO

ExPResSO is a C library available as both a static library (`libexpresso.a`) and a shared library (`libexpresso.so`).

## Requirements

- A C compiler (e.g. `gcc` or `clang`)
- `make`
- `git`

## Getting the code

```bash
git clone https://github.com/theob55/ExPResSO.git
cd ExPResSO
```

## Building

From the root of the repository:

```bash
make
```

This is the default target (`all`). It:

1. compiles the source file `expresso.c` into the object file `expresso.o`,
2. builds ExPResSO as a static library, `libexpresso.a`,
3. builds ExPResSO as a dynamic (shared) library, `libexpresso.so`.

## Running the tests

From the root of the repository:

```bash
make test
```

This compiles and runs every test program in the `tests/` directory.

## Installing

Install the library to the location given by the `PREFIX` variable (see the `Makefile` for its default value):

```bash
make install
```

To install somewhere specific, set `PREFIX` on the command line. For example, to install under your home directory:

```bash
PREFIX=$HOME/bin/expresso make install
```

To remove the installed files, use the same `PREFIX`:

```bash
PREFIX=$HOME/bin/expresso make uninstall
```

## Cleaning

Remove the files produced by the build:

```bash
make clean
```

## Make targets

### Root `Makefile`

| Target      | Description                                                              |
|-------------|--------------------------------------------------------------------------|
| `all`       | (default) Builds `expresso.o`, `libexpresso.a` and `libexpresso.so`       |
| `install`   | Installs ExPResSO to the location set by `PREFIX`                        |
| `uninstall` | Removes ExPResSO from the location set by `PREFIX`                       |
| `test`      | Compiles and runs all the tests in `tests/`                              |
| `clean`     | Removes the files produced by the build                                  |

### `tests/Makefile`

You can also work directly from the `tests/` directory:

| Target  | Description                                                                                                                                      |
|---------|--------------------------------------------------------------------------------------------------------------------------------------------------|
| `all`   | (default) Builds every test program and links it against the static library `libexpresso.a`                                                        |
| `run`   | Runs every test program under several ExPResSO configurations (number of worker threads, scheduling policy)                                       |
| `clean` | Removes the files produced when building the test programs                                                                                       |

> `libexpresso.a` must be built first (run `make` at the repository root).

## Repository layout

```
.
├── Makefile        # Build, install and test recipes
├── expresso.c      # Library source
├── tests/          # Test programs and their own Makefile
└── ...
```
