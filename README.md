# render-workbook

A self-built, university-course-style curriculum for learning **Vulkan**,
**compute shaders**, **Lua embedding**, and **Direct3D 12** — by filling in
holes in a real codebase rather than reading tutorials.

Every task ships with a skeleton, a layered automatic grader, and a reference
solution you can choose not to look at. Windows and macOS. C++17 + CMake.

## What makes it different from a tutorial

**Layered grading, not "does it look right".**

| Layer | Mechanism | Determinism |
|---|---|---|
| **L1 validation** | Vulkan validation layer (including *synchronization validation*) must report zero errors and zero warnings | High |
| **L2 readback** | GPU results read back and compared element-wise against a CPU reference implementation | **100%** |
| **L3 golden image** | Rendered frame compared against a baseline | Depends on baseline |

L1 is the layer that matters most. A Vulkan program can produce a perfectly
correct-looking image while its synchronization is wrong — that class of bug is
the expensive one, and only the validation layer catches it.

L3 baselines are generated **exclusively by lavapipe** (Mesa's pure-CPU software
rasterizer) in CI. It is the only renderer that produces identical pixels on
NVIDIA, Apple Silicon, and CI alike. On a local GPU the harness automatically
relaxes the tolerance and treats L3 as a smoke test.

For scenes dominated by texture filtering or multisampling, even a relaxed
per-pixel tolerance is the wrong instrument: anisotropic filtering is a
vendor-private implementation, so NVIDIA's 16x and lavapipe's none-at-all
disagree by far more than any sane threshold — without either being wrong.
Those cases fall back to a *structural* comparison (16x16 block averages) on
real GPUs, which still catches a broken composition. Strict per-pixel grading
stays where it belongs: lavapipe in CI.

**Skeletons must compile.** Every `start/*` tag is verified to build cleanly and
fail its tests legibly. If you cannot compile, you cannot even start the task.

## Curriculum

| Project | Topic | Hours | Status |
|---|---|---|---|
| P0 | Environment self-check | 8–12 | Shipped |
| P1 | Triangle from scratch — 9 tasks, nothing pre-written | 35–45 | Shipped |
| P2 | Resources & scene (VMA, descriptors, textures, glTF, MSAA) | 45–60 | Shipped |
| P3 | Compute shaders (reduction, scan, bitonic sort, particles) | 35–45 | Shipped |
| P4 | Deferred rendering + PBR + IBL, light-space shadows, migration to Slang | 45–60 | t01–t07 implemented locally; release tags pending |
| P5 | Render graph with automatic barrier derivation | 50–70 | Outlined |
| P6 | Lua: language, then embedding as the engine's scripting layer | 40–55 | Outlined |
| P7 | D3D12 concept walkthrough (Windows only) | 35–45 | Outlined |

Roughly 300–390 hours end to end.

## Layout

```
engine/        Shared infrastructure
  core/        logging, files, validation sink, the NotImplemented exception
  platform/    GLFW window
  rhi/         Vulkan wrapper — this layer *is* the output of P1: instance,
               device, swapchain, sync and presentation, extracted after you
               have written them once by hand
projects/      One directory per project; src/steps/0N_*.cpp = one file per task
tests/         Grading framework: ImageCompare (L3), BufferAssert (L2)
cmake/         SHA256-pinned dependencies, shader compilation, test registration
docs/          Roadmap, per-platform setup, git workflow, symptom lookup table
```

## Build

```bash
python rwb.py doctor          # show detected host, preset, and tools
python rwb.py test p00        # configure + build + environment self-test
```

On macOS, use `python3` if `python` is not defined. The launcher selects
`win-msvc`, `mac-arm64`, or `ci-lavapipe` from the host system. Its three steps
are deliberately visible:

- **configure** (`cmake --preset ...`) generates a platform build tree;
- **build** (`cmake --build ...`) compiles targets in that tree;
- **test** (`ctest --preset ...`) runs already-built tests.

Use `python rwb.py --dry-run test p01-t03` to print those commands without
executing them, or `python rwb.py --help` for all actions.

Most dependencies are fetched by CMake and pinned by SHA256 — GLFW, glm,
Vulkan-Headers, volk, glslang, Catch2, stb. P0–P3 compile without an SDK;
P4-t07 additionally requires `slangc` from a Vulkan SDK. Running L1 tests also
requires the SDK's validation layer.

Setup details: [Windows](docs/01-setup-windows.md) · [macOS](docs/01-setup-macos.md)

Course maintenance: [任务编写与反馈检查点规范](docs/04-course-authoring.md)

## Working through a task

```bash
git checkout -B work start/p01-t03    # check out the task
# ... fill in the TODOs ...
python rwb.py test p01-t03             # build and grade on this host
git diff done/p01-t03 -- projects/p01-triangle/src/steps/03_swapchain.cpp
```

Two long-lived branches (`course`, `work`) plus tags — not one branch per task.
The reasoning is in [docs/02-git-workflow.md](docs/02-git-workflow.md).

## Documentation language

Task briefs and in-code comments are in Chinese; this README is in English.
