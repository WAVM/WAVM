# Build System & CI

WAVM uses `Build/dev.py` to manage multi-configuration builds, testing, formatting, packaging, and coverage. The same script drives both local development and GitHub Actions CI.

## Quick Reference

```bash
# Build and test all configurations
python3 Build/dev.py test [--work-dir DIR]

# Build and test a single configuration
python3 Build/dev.py test --config Debug

# List available configurations for your platform
python3 Build/dev.py list-configs

# Check C/C++ source formatting (clang-format)
python3 Build/dev.py format

# Auto-fix C/C++ source formatting
python3 Build/dev.py format --fix

# Run Python linters (ruff + ty)
python3 Build/dev.py lint

# Run clang-tidy
python3 Build/dev.py tidy

# Auto-fix clang-tidy issues
python3 Build/dev.py tidy --fix

# Build packages for a configuration
python3 Build/dev.py package --config LTO output_dir

# Test a pre-built installation
python3 Build/dev.py test-install --config LTO install_dir

# Build and run a program (default config: LTO)
python3 Build/dev.py run [--config Debug] -- wavm test script Test/wavm/simd.wast

# Generate/update VS Code workspace with clangd config
python3 Build/dev.py setup-workspace path/to/wavm.code-workspace

# Merge coverage files from multiple runs
python3 Build/dev.py merge-coverage --output-dir DIR file1.lcov file2.lcov ...
```

### Global Options

| Option | Description |
|--------|-------------|
| `--work-dir DIR` | Working directory for builds, LLVM, etc. Default: `$WAVM_BUILD_TOOL_WORK_DIR` or `.working/` |
| `--verbose` / `-v` | Print task start/completion details |
| `--llvm-source` | Build LLVM from source instead of downloading pre-built binaries |
| `--offline` | Skip all network operations; use previously downloaded/cloned data |

## Configurations

Each configuration is named `{template}[-{compiler}[-{linker}]]` (e.g., `Debug`, `RelWithDebInfo-clang-link`).

### Configuration Templates

| Template | Build Type | Description | Restrictions |
|----------|-----------|-------------|--------------|
| **RelWithDebInfo** | RelWithDebInfo | Release with debug info | Expanded across all available linkers |
| **Debug** | Debug | Full debug build | |
| **Checked** | RelWithDebInfo | Release with asserts, fuzz targets, coverage (clang) | |
| **Static** | Release | Statically linked | |
| **LTO** | Release | Thin LTO (clang) or full LTO (MSVC) | Clang or MSVC only |
| **StaticLTO** | Release | Static + Thin LTO | Clang, POSIX only |
| **UBASAN** | RelWithDebInfo | Address + undefined behavior sanitizers | Clang, Linux only |
| **TSAN** | Release | Thread sanitizer | Clang, POSIX only |
| **NoRuntime** | RelWithDebInfo | Parse/validate only, no LLVM needed | |

### Detected Compilers and Linkers

Compilers and linkers are auto-detected:

- **Compilers:** `clang` (always), `gcc` (Linux), `msvc` (Windows)
- **Linkers:** `default` (always), `lld` (if available), `gold` (Linux, if available)

RelWithDebInfo is expanded across all compiler+linker combinations to test linker compatibility. Other configurations use the preferred linker (lld if available, otherwise default).

## Working Directory Structure

```
work-dir/
├── build/
│   ├── Debug/           # Build directory per configuration
│   │   ├── bin/wavm           # Built binaries
│   │   ├── .cmake_config_args # Cached CMake args (for incremental builds)
│   │   └── .build_stamp       # Config hash + LLVM commit
│   ├── Checked/
│   └── ...
├── wavm-install/
│   ├── Debug/           # Install directory per configuration
│   └── ...
├── llvm/
│   └── llvm-install/          # Downloaded or built LLVM installations
│       ├── {platform}-LTO/
│       ├── {platform}-Debug/
│       └── ...
├── wavm-fuzz-corpora/         # Cloned fuzz corpora (if fuzz tests run)
├── wasi-sdk-29/               # Downloaded WASI SDK
├── wasi-wasm/                 # Compiled WASI test modules
├── coverage/                  # Coverage profraw files
├── coverage-report/           # HTML coverage report
├── coverage.lcov              # LCOV format coverage
└── test_history.json          # Test result history for prioritization
```

### Incremental Builds

The build system detects when CMake reconfiguration is needed by comparing the current CMake arguments and LLVM commit hash against stored values in `.cmake_config_args` and `.build_stamp`. If nothing has changed and `CMakeCache.txt` exists, the `cmake --fresh` step is skipped.

After the initial `dev.py` run, you can rebuild a single configuration directly:

```bash
ninja -C ~/.working/wavm/build/Debug
```

## LLVM

WAVM requires LLVM 21 (from the [WAVM-LLVM](https://github.com/WAVM/WAVM-LLVM) fork with WAVM-specific patches).

By default, `dev.py` downloads pre-built LLVM binaries from WAVM-LLVM GitHub releases. Use `--llvm-source` to build LLVM from source instead.

Each WAVM configuration uses a specific LLVM configuration:

| WAVM Config | LLVM Config |
|-------------|-------------|
| Debug | Debug |
| RelWithDebInfo, Static, TSAN | RelWithDebInfo |
| LTO, StaticLTO | LTO |
| Checked | Checked |
| UBASAN | Sanitized |
| NoRuntime | (none) |

## Testing

### Test Suites

Tests are automatically filtered based on configuration capabilities (runtime enabled, static/shared linking, sanitizer status).

**Unit tests:** HashMap, HashSet, I128, LEB128, C-API, API, version output validation

**WAST script tests:** Discovered from `Test/wavm/`, `Test/WebAssembly/spec/`, `Test/WebAssembly/memory64/`, `Test/WebAssembly/threads/`, `Test/WebAssembly/multi-memory/`

**Run tests:** Example programs (blake2b, trap, zlib)

**Embedder tests:** C and C++ embedding examples (requires shared build, non-sanitized)

**WASI tests:** 45+ tests covering file I/O, filesystem operations, POSIX interfaces, and out-of-bounds pointer handling. Compiled from C++ source using WASI SDK 29.

**Fuzz tests:** Corpus-based fuzzing for assemble, disassemble, compile-model, and instantiate (requires Checked or LTO config)

**GDB tests:** Verifies GDB can read JIT-compiled function names (Linux only)

### Test Prioritization

Test history is stored in `test_history.json`. Tests are prioritized:
1. Previously failed tests run first
2. Tests with no history run second
3. Previously passed tests run last

Within the same priority level, slower tests run first to maximize parallelism.

### Parallel Execution

Tests run in parallel using a thread pool with `cpu_count()` workers. Build tasks (`BUILD` type) are serialized; test tasks (`GENERAL` type) run concurrently.

## Formatting

Format checking uses `clang-format` from the LLVM LTO toolchain. It discovers all `.h`, `.cpp`, and `.c` files, excluding `.git/`, `.working/`, `ThirdParty/`, and `Include/WAVM/Inline/xxhash/`.

In check mode, differences are shown as colorized unified diffs. In fix mode (`--fix`), files are modified in-place.

## Coverage

Coverage is collected on the Checked configuration using Clang's source-based code coverage (`-DWAVM_ENABLE_COVERAGE=ON`).

During test execution, `LLVM_PROFILE_FILE` is set per test step to generate `.profraw` files. After testing:

1. `llvm-profdata merge` combines profraw files
2. `llvm-cov show` generates HTML and annotated-source reports
3. `llvm-cov export` generates an LCOV file

The `merge-coverage` command can combine LCOV files from multiple platforms, summing hit counts for shared lines.

## Packaging

`dev.py package` uses CMake/CPack to create platform-specific packages:

| Platform | Formats |
|----------|---------|
| Linux | .tar.gz, .deb, .rpm |
| Windows | .zip, .exe (NSIS installer) |
| macOS | .tar.gz |

Release packages use the LTO configuration for optimized binary size and performance.

## GitHub Actions CI

The CI workflow (`.github/workflows/build.yml`) is triggered by pushes to `master`, pull requests against `master`, version tags (`v*`), nightly schedule (6 AM UTC), and manual dispatch.

### Job Phases

```
Phase 1: Detect configurations (parallel, per platform)
  get-linux-configs, get-windows-configs, get-macos-configs
  → Runs: dev.py list-configs

Phase 2: Format + Lint + Tidy (parallel with Phase 1)
  lint
  → Runs: dev.py format
  → Runs: dev.py lint
  → Runs: dev.py tidy

Phase 3: Build, test, and package (parallel, per config)
  linux-build-test-package, windows-build-test-package, macos-build-test-package
  → Runs: dev.py test --config <config>
  → Runs: dev.py package --config <config> (for packageable configs)

Phase 4: Package testing (parallel, per package format)
  linux-test-distros (11 distributions), linux-test-deb, linux-test-rpm,
  windows-test-exe, windows-test-zip, macos-test-tgz
  → Runs: dev.py test-install --config <config> <install_dir>

Phase 5: Coverage merge (runs even on failure)
  → Runs: dev.py merge-coverage

Phase 6: Release publishing (version tags only)
  → Renames packages with version, uploads to GitHub release
```

### Containers

| Container | Used By | Purpose |
|-----------|---------|---------|
| `ghcr.io/wavm/builder-linux-x64:latest` | Linux config detection, format check, Linux builds | Build toolchain |
| `ghcr.io/wavm/tester:<distro>` | Distro testing, deb/rpm testing | Clean OS environment |

### Linux Distribution Testing

LTO packages are tested on 11 distributions to verify portability:

- **Debian-based:** Ubuntu 20.04, 22.04, 24.04; Debian 11, 12
- **RPM-based:** Fedora 40, 41; AlmaLinux 8, 9; openSUSE Leap 15.6
- **Other:** Arch

Tarballs are tested on all 11. `.deb` packages are tested on the 5 Debian-based distros. `.rpm` packages are tested on the 5 RPM-based distros.

### Artifact Retention

- Build artifacts and packages: 1 day (deleted after package testing completes)
- Coverage data: 7 days

## Build System Source

The build system is implemented in Python in `Build/`:

| File | Purpose |
|------|---------|
| `dev.py` | Entry point, argument parsing, command dispatch |
| `lib/build.py` | Configuration templates, CMake invocation, compiler/linker detection |
| `lib/test.py` | Test definitions, test step execution, result tracking |
| `lib/check.py` | Source file discovery, clang-format, Python linting |
| `lib/tidy.py` | Clang-tidy |
| `lib/coverage.py` | Coverage merging and report generation |
| `lib/llvm.py` | LLVM download/build management |
| `lib/wasi.py` | WASI SDK download, test module compilation |
| `lib/task_graph.py` | Parallel task execution with dependency tracking |
| `lib/platform.py` | Platform detection, CPack configuration |
| `lib/output.py` | Console output formatting and status display |
| `lib/toolchain.py` | Toolchain detection, configuration, and CMake argument generation |
| `lib/test_def.py` | Test definition types (TestContext, TestResult, TestStep, TestDef) |
| `lib/workspace.py` | VS Code workspace file generation for clangd integration |
