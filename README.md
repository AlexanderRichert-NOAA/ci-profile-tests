# ci-multiprofiler

A **unified profiling framework** for CMake/CTest projects on GitHub Actions and
local workstations.  It supports three back-ends:

| Back-end | Tool(s) | Method |
|---|---|---|
| **gprof** | GNU `gprof` | `-pg` instrumentation; per-test `gmon` files |
| **vtune** | Intel VTune Profiler | Hardware counters; per-test result directory |
| **hpctoolkit** | `hpcrun` + `hpcstruct` + `hpcprof` | Sampling; per-test database |

---

## Directory layout

```
ci-multiprofiler/
├── action.yml                   # Composite GitHub Action
├── cmake/
│   └── Profiling.cmake          # CMake module — include in your project
├── .github/
│   └── workflows/
│       └── profile-workflow.yml # Reusable GitHub Workflow
└── README.md
```

---

## Integration guide

### 1  Pull `Profiling.cmake` into your project via `FetchContent`

Add the following block to your project's top-level `CMakeLists.txt`
**before** any `add_test()` calls (typically just after `enable_testing()`):

```cmake
cmake_minimum_required(VERSION 3.19)   # minimum required by Profiling.cmake
project(MyProject C CXX Fortran)

enable_testing()

# ── Pull in the profiling framework only when profiling is requested ───────
option(ENABLE_PROFILING "Enable profiling wrappers for CTest tests" OFF)

if(ENABLE_PROFILING)
    include(FetchContent)

    FetchContent_Declare(
        ci_multiprofiler
        GIT_REPOSITORY https://github.com/<your-org>/ci-multiprofiler.git
        GIT_TAG        main          # pin to a tag/SHA for reproducibility
    )
    FetchContent_MakeAvailable(ci_multiprofiler)

    include("${ci_multiprofiler_SOURCE_DIR}/cmake/Profiling.cmake")
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
| `PROFILING_TOOL` | `gprof` | Back-end: `vtune`, `gprof`, or `hpctoolkit` |
| `PROFILING_OUTPUT_DIR` | `<build>/profiling-results` | Root output directory |

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
#    Each test produces two files in build-prof/profiling-results/<test_name>/:
#      gmon.<pid>      ← raw gmon binary (input for gprof)
#      gmon.<pid>.txt  ← human-readable gprof flat profile + call graph
ls build-prof/profiling-results/solver_small/
# gmon.12345      gmon.12345.txt
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
cmake -S . -B build-hpct \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DENABLE_PROFILING=ON \
      -DPROFILING_TOOL=hpctoolkit

cmake --build build-hpct --parallel
ctest --test-dir build-hpct --output-on-failure

# Browse the database:
hpcviewer build-hpct/profiling-results/solver_small/hpctoolkit-database
```

HPCToolkit understands Fortran DWARF (`DW_TAG_module`) and displays
module-qualified procedure names natively in `hpcviewer`.

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
        description: 'vtune | gprof | hpctoolkit'
        default: 'gprof'

jobs:
  profile:
    uses: <your-org>/ci-multiprofiler/.github/workflows/profile-workflow.yml@main
    with:
      profiling_tool: ${{ github.event.inputs.profiling_tool || 'gprof' }}
      test_regex:     '^solver'           # optional: only run matching tests
      cmake_args:     '-DSOME_OPTION=ON'  # optional: extra CMake flags
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
      - uses: <your-org>/ci-multiprofiler@<ref>
        with:
          profiling_tool: hpctoolkit
          checkout:        'true'
          source_dir:      '.'
          build_dir:       'build'
          cmake_args:      '-DMPI=ON'
          test_regex:      'mpi_'
          artifact_retention_days: '30'
```

---

## How artifact isolation works

A key design goal is that running `make test` or `ctest` multiple times, or
running tests in parallel, must **never** clobber profiling data.

| Back-end | Isolation mechanism |
|---|---|
| **gprof** | `GMON_OUT_PREFIX=<outdir>/gmon` — glibc writes `gmon.<pid>` instead of `gmon.out`; the test runs in a private `mktemp` scratch directory. |
| **vtune** | Each test uses `-result-dir <outdir>/vtune-result`; `-allow-multiple-runs` lets VTune append data across repeated runs without overwriting. |
| **hpctoolkit** | Each test writes to `<outdir>/measurements/`; structure and database go to `<outdir>/structs/` and `<outdir>/hpctoolkit-database/`. |

`<outdir>` is always `PROFILING_OUTPUT_DIR/<test_name_as_c_identifier>` so
tests that share the same binary but differ in arguments each get their own
directory.

---

## Fortran considerations

- `-pg -g` (gprof) and `-g` (VTune/HPCToolkit) are automatically appended to
  `CMAKE_C_FLAGS`, `CMAKE_CXX_FLAGS`, and `CMAKE_Fortran_FLAGS`.
- `-rdynamic` is added to `CMAKE_EXE_LINKER_FLAGS` for VTune and HPCToolkit so
  dynamic symbols are resolvable in profiles.
- For gprof, `-pg` is also added to `CMAKE_EXE_LINKER_FLAGS` and
  `CMAKE_SHARED_LINKER_FLAGS` so the profiling startup code is linked
  independently of which compiler driver is used as the linker.
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
| GitHub Actions runner | ubuntu-22.04 or newer |
