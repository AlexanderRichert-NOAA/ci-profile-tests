# ci-profile-tests

A **unified profiling framework** for CMake/CTest projects on GitHub Actions and
local workstations.  It supports four back-ends:

| Back-end | Tool(s) | Method |
|---|---|---|
| **gprof** | GNU `gprof` | `-pg` instrumentation; per-test `gmon` files |
| **vtune** | Intel VTune Profiler | Hardware counters; per-test result directory |
| **hpctoolkit** | `hpcrun` + `hpcstruct` + `hpcprof` | Sampling; per-test database |
| **scalasca** | `scalasca` + Score-P | Automatic instrumentation via `RULE_LAUNCH`; per-test experiment archive |

---

## Directory layout

```
ci-profile-tests/
├── action.yml                   # Composite GitHub Action
├── cmake/
│   └── Profiling.cmake          # CMake module — include in your project
├── test/
│   ├── CMakeLists.txt           # Self-test project for the framework
│   ├── compute.c / compute.h
│   ├── main.c
│   └── sum.f90
├── .github/
│   └── workflows/
│       ├── ci.yml               # Framework's own CI self-tests
│       └── profile-workflow.yml # Reusable GitHub Workflow
└── README.md
```

---

## Integration guide

### 1  Pull `Profiling.cmake` into your project via `FetchContent`

Add the following block to your project's top-level `CMakeLists.txt`
**before** any `add_test()` calls (typically just after `enable_testing()`):

```cmake
cmake_minimum_required(VERSION 3.12)   # minimum required by Profiling.cmake
project(MyProject C CXX Fortran)

enable_testing()

# ── Pull in the profiling framework only when profiling is requested ───────
option(ENABLE_PROFILING "Enable profiling wrappers for CTest tests" OFF)

if(ENABLE_PROFILING)
    include(FetchContent)

    FetchContent_Declare(
        ci_profile_tests
        GIT_REPOSITORY https://github.com/AlexanderRichert-NOAA/ci-profile-tests.git
        GIT_TAG        main          # pin to a tag/SHA for reproducibility
    )
    FetchContent_MakeAvailable(ci_profile_tests)

    include("${ci_profile_tests_SOURCE_DIR}/cmake/Profiling.cmake")
endif()

# ── Define your targets and tests as usual ────────────────────────────────
add_executable(my_solver solver.c solver_f.f90)

add_test(NAME solver_small COMMAND my_solver --input small.dat)
add_test(NAME solver_large COMMAND my_solver --input large.dat)
```

---

### 2  CMake options reference

| CMake option | Default | Description |
|---|---|---|
| `ENABLE_PROFILING` | `OFF` | Master switch — wrap all CTest tests |
| `PROFILING_TOOL` | `gprof` | Back-end: `vtune`, `gprof`, `hpctoolkit`, or `scalasca` |
| `PROFILING_OUTPUT_DIR` | `<build>/profiling-results` | Root output directory |
| `PROFILING_ANALYSIS` | `OFF` | Post-process raw data into a human-readable report (gprof / VTune hotspots / `hpcstruct` + `hpcprof` / `scalasca -examine`) |

---

## Running locally

### gprof

```bash
# 1. Configure with profiling enabled
cmake -S . -B build-prof \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DENABLE_PROFILING=ON \
      -DPROFILING_TOOL=gprof

# 2. Build
cmake --build build-prof --parallel

# 3. Run tests (wrapper scripts are generated in build-prof/_profiling_wrappers/)
ctest --test-dir build-prof --output-on-failure

# 4. Inspect results
#    Each test produces a raw gmon binary in build-prof/profiling-results/<test_name>/:
#      gmon.<pid>      ← raw gmon binary (input for gprof)
#    To also generate human-readable flat profiles and call graphs, add
#    -DPROFILING_ANALYSIS=ON to the cmake configure step.  That produces:
#      gmon.<pid>.gprof_output.txt  ← human-readable gprof flat profile + call graph
ls build-prof/profiling-results/solver_small/
# gmon.12345
# (with -DPROFILING_ANALYSIS=ON: gmon.12345.gprof_output.txt also appears)
```


### Intel VTune

> **Prerequisite — `ptrace_scope`**
> VTune's collection agent must attach to the profiled process via `ptrace`.
> On Linux systems with YAMA LSM this requires `ptrace_scope` to be set to `0`:
>
> ```bash
> # Temporary (reverts on reboot):
> sudo sh -c 'echo 0 > /proc/sys/kernel/yama/ptrace_scope'
>
> # Permanent (requires reboot):
> echo 'kernel.yama.ptrace_scope = 0' | \
>     sudo tee /etc/sysctl.d/10-ptrace.conf
> ```
>
> GitHub Actions `ubuntu-*` runners have `ptrace_scope=0` by default.

```bash
cmake -S . -B build-vtune \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DENABLE_PROFILING=ON \
      -DPROFILING_TOOL=vtune

cmake --build build-vtune --parallel
ctest --test-dir build-vtune --output-on-failure

# Open the result in the VTune GUI:
vtune-gui build-vtune/profiling-results/solver_small/vtune-result
```

### HPCToolkit

```bash
cmake -S . -B build-hpctoolkit \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DENABLE_PROFILING=ON \
      -DPROFILING_TOOL=hpctoolkit
      -DPROFILING_ANALYSIS=ON

cmake --build build-hpctoolkit --parallel
ctest --test-dir build-hpctoolkit --output-on-failure

# Browse the database (only present with -DPROFILING_ANALYSIS=ON):
hpcviewer build-hpctoolkit/profiling-results/solver_small/hpctoolkit-database
```

HPCToolkit understands Fortran DWARF (`DW_TAG_module`) and displays
module-qualified procedure names natively in `hpcviewer`.

### Scalasca

```bash
cmake -S . -B build-scalasca \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DENABLE_PROFILING=ON \
      -DPROFILING_TOOL=scalasca

cmake --build build-scalasca --parallel
ctest --test-dir build-scalasca --output-on-failure

# Per-test experiment archives land in:
#   build-scalasca/profiling-results/<test_name>/scorep_archive/
# With -DPROFILING_ANALYSIS=ON, scalasca -examine is run automatically and
# a human-readable summary is written to:
#   build-scalasca/profiling-results/<test_name>/scalasca_summary.txt
# You can also explore interactively:
scalasca -examine build-scalasca/profiling-results/solver_small/scorep_archive
```

---

## GitHub Actions integration

### A  Use the reusable workflow from a caller workflow

In your project repository create (or extend)
`.github/workflows/profile.yml`:

```yaml
name: Profile

on:
  push:
    branches: [main]
  workflow_dispatch:
    inputs:
      profiling_tool:
        description: 'vtune | gprof | hpctoolkit | scalasca'
        default: 'gprof'

jobs:
  profile:
    uses: AlexanderRichert-NOAA/ci-profile-tests/.github/workflows/profile-workflow.yml@<ref>
    with:
      profiling_tool: ${{ github.event.inputs.profiling_tool || 'gprof' }}
      test_regex:     '^solver'           # optional: only run matching tests
      cmake_args:     '-DSOME_OPTION=ON'  # optional: extra CMake flags
```

For Scalasca, pass `scalasca_version` if you need a specific release:

```yaml
    with:
      profiling_tool:   scalasca
      scalasca_version: '2.6'   # optional; omit to let Spack choose latest
```

The reusable workflow:
1. Runs on `ubuntu-latest` (override via `runner` input for self-hosted nodes).
2. Checks out your code.
3. Installs the requested profiler (if not already present).
4. Builds with `RelWithDebInfo` + profiling flags.
5. Runs `ctest -R <test_regex>`.
6. Uploads all files under `build/profiling-results/` as a GitHub Actions
   artifact named `profiling-results-<tool>-<run_id>`.

### B  Use the composite action directly

To use the custom action directly:

```yaml
jobs:
  profile:
    steps:
      - uses: AlexanderRichert-NOAA/ci-profile-tests@<ref>
        with:
          profiling_tool: hpctoolkit
          checkout:        'true'
          source_dir:      '.'
          build_dir:       'build'
          cmake_args:      '-DMPI=ON'
          test_regex:      'mpi_'
          artifact_retention_days: '30'
          label_regex:      ''         # optional: only tests with matching labels
          label_exclude:    ''         # optional: exclude tests with matching labels
          cache_tools:      'false'    # set to 'true' to cache profiler installs
          hpctoolkit_version: '2024.01.1'  # HPCToolkit release version (if used)
          vtune_version:    '2025.10'  # Intel oneAPI VTune version (if used)
          scalasca_version: '2.6'     # Scalasca version (if used; leave blank for latest)
```

---

## How artifact isolation works

A key design goal is that running `make test` or `ctest` multiple times, or
running tests in parallel, must **never** clobber profiling data.

| Back-end | Isolation mechanism |
|---|---|
| **gprof** | `GMON_OUT_PREFIX=<outdir>/gmon` — glibc writes `gmon.<pid>` instead of `gmon.out`; the test runs in a private `mktemp` scratch directory. |
| **vtune** | Each test uses `-result-dir <outdir>/vtune-result`; `-allow-multiple-runs` lets VTune append data across repeated runs without overwriting. |
| **hpctoolkit** | Each test writes to `<outdir>/measurements/` via `hpcrun`.  With `PROFILING_ANALYSIS=ON`, a dependent `_hpctoolkit_analysis` test automatically runs `hpcstruct` + `hpcprof`, producing `<outdir>/structs/` and `<outdir>/hpctoolkit-database/`. |
| **scalasca** | `SCOREP_EXPERIMENT_DIRECTORY=<outdir>/scorep_archive` pins the Score-P experiment archive to a unique per-test path.  With `PROFILING_ANALYSIS=ON`, a dependent `_scalasca_analysis` test runs `scalasca -examine -s` and writes `<outdir>/scalasca_summary.txt`. |

`<outdir>` is always `PROFILING_OUTPUT_DIR/<test_name_as_c_identifier>` so
tests that share the same binary but differ in arguments each get their own
directory.

---

## Fortran considerations

- `-pg -g` (gprof) and `-g` (VTune/HPCToolkit/Scalasca) are automatically appended to
  `CMAKE_C_FLAGS`, `CMAKE_CXX_FLAGS`, and `CMAKE_Fortran_FLAGS`.
- `-rdynamic` is added to `CMAKE_EXE_LINKER_FLAGS` for VTune and HPCToolkit so
  dynamic symbols are resolvable in profiles.
- For gprof, `-pg` is also added to `CMAKE_EXE_LINKER_FLAGS` and
  `CMAKE_SHARED_LINKER_FLAGS` so the profiling startup code is linked
  independently of which compiler driver is used as the linker.
- For Scalasca, `RULE_LAUNCH_COMPILE` and `RULE_LAUNCH_LINK` are set to
  `scalasca -instrument`, which wraps every compile and link command so that
  Score-P inserts its hooks and links the measurement library.  This works with
  both the Makefile and Ninja generators and is compatible with mixed-language
  (C + Fortran) projects.
- HPCToolkit `hpcstruct` parses DWARF `DW_TAG_module` entries produced by
  gfortran and ifort, so Fortran module/procedure hierarchies are preserved in
  the final `hpcviewer` display.

---

## Requirements

| Component | Minimum version |
|---|---|
| CMake | 3.12 |
| GNU binutils (`gprof`) | any recent version |
| Intel VTune | oneAPI 2021+; `/proc/sys/kernel/yama/ptrace_scope` = 0 |
| HPCToolkit | 2020+ |
| Scalasca + Score-P | 2.x |
| GitHub Actions runner (action.yml/profile-workflow.yml) | ubuntu-22.04 or newer |
