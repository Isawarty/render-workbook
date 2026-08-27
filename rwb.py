#!/usr/bin/env python3
"""Small cross-platform front end for render-workbook's CMake workflow."""

from __future__ import annotations

import argparse
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Sequence


ROOT = Path(__file__).resolve().parent


@dataclass(frozen=True)
class Project:
    test_target: str
    app_target: Optional[str] = None
    executable_dir: Optional[Path] = None
    # Number of graded tasks (pXX-tNN). 0 means the project has no task numbers.
    tasks: int = 0
    # The application understands "--frames N" (keep rendering, then exit).
    accepts_frames: bool = False
    # The application takes a positional stage number 1..stages.
    stages: int = 0


PROJECTS = {
    "p00": Project(test_target="p00_tests", app_target="p00_setup",
                   executable_dir=Path("projects/p00-setup")),
    "p01": Project(test_target="p01_tests", app_target="p01_triangle",
                   executable_dir=Path("projects/p01-triangle"),
                   tasks=9, accepts_frames=True),
    "p02": Project(test_target="p02_tests", app_target="p02_resources",
                   executable_dir=Path("projects/p02-resources"),
                   tasks=8, stages=8),
}

SYSTEM_PRESETS = {
    "Windows": "win-msvc",
    "Darwin": "mac-arm64",
    "Linux": "ci-lavapipe",
}


class UserError(RuntimeError):
    """An actionable command-line error that should not print a traceback."""


def detect_preset(system: Optional[str] = None, machine: Optional[str] = None) -> str:
    system = system or platform.system()
    machine = (machine or platform.machine()).lower()
    try:
        preset = SYSTEM_PRESETS[system]
    except KeyError as exc:
        supported = ", ".join(SYSTEM_PRESETS)
        raise UserError(f"unsupported host system {system!r}; supported: {supported}") from exc

    if system == "Darwin" and machine not in {"arm64", "aarch64"}:
        raise UserError(
            f"mac-arm64 requires Apple Silicon, but this machine reports {machine!r}"
        )
    return preset


def command_text(command: Sequence[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(list(command))
    return shlex.join(command)


def run_command(command: Sequence[str], *, dry_run: bool) -> None:
    print(f"> {command_text(command)}", flush=True)
    if not dry_run:
        subprocess.run(list(command), cwd=ROOT, check=True)


def find_tool(name: str) -> Optional[str]:
    found = shutil.which(name)
    if found:
        return found

    python_dir = Path(sys.executable).resolve().parent
    candidates = [python_dir / name]
    if os.name == "nt":
        candidates = [python_dir / "Scripts" / f"{name}.exe", python_dir / f"{name}.exe"]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    return None


def require_tool(name: str, *, dry_run: bool) -> str:
    if dry_run:
        return name
    found = find_tool(name)
    if found is None:
        raise UserError(
            f"{name!r} is not on PATH. Follow docs/01-setup-windows.md or "
            "docs/01-setup-macos.md first."
        )
    return found


def check_toolchain(preset: str, *, dry_run: bool) -> str:
    cmake = require_tool("cmake", dry_run=dry_run)
    if preset == "win-msvc" and not dry_run and shutil.which("cl") is None:
        raise UserError(
            "win-msvc needs the MSVC environment. Run this command from an "
            "x64 Native Tools terminal (or call vcvars64.bat first)."
        )
    return cmake


def configure(preset: str, *, reconfigure: bool, dry_run: bool) -> None:
    cmake = check_toolchain(preset, dry_run=dry_run)
    cache = ROOT / "build" / preset / "CMakeCache.txt"
    if cache.exists() and not reconfigure:
        print(f"[configure] reuse {cache.relative_to(ROOT)}")
        return
    print(f"[configure] generate build/{preset}")
    run_command([cmake, "--preset", preset], dry_run=dry_run)


def build(project_name: str, preset: str, *, reconfigure: bool, dry_run: bool,
          app: bool = False) -> None:
    project = PROJECTS[project_name]
    configure(preset, reconfigure=reconfigure, dry_run=dry_run)
    target = project.app_target if app else project.test_target
    if target is None:
        raise UserError(f"{project_name} has no runnable application target")
    print(f"[build] target {target}")
    cmake = require_tool("cmake", dry_run=dry_run)
    run_command(
        [cmake, "--build", "--preset", preset, "--target", target],
        dry_run=dry_run,
    )


def parse_selection(selection: str) -> tuple[str, Optional[str]]:
    selection = selection.lower()
    match = re.fullmatch(r"(p\d{2})(?:-t(\d{2})(?:-cp(\d{2}))?)?", selection)
    if not match:
        raise UserError(
            "test selection must look like 'p02', 'p02-t04', or 'p02-t04-cp01'"
        )

    project_name, task, checkpoint = match.groups()
    if project_name not in PROJECTS:
        known = ", ".join(PROJECTS)
        raise UserError(f"{project_name} is not available yet; known projects: {known}")

    project = PROJECTS[project_name]
    if task is not None:
        if project.tasks == 0:
            raise UserError(f"{project_name} has no numbered tasks")
        if not 1 <= int(task) <= project.tasks:
            raise UserError(
                f"{project_name} task number must be between t01 and "
                f"t{project.tasks:02d}"
            )
    if checkpoint is not None and int(checkpoint) < 1:
        raise UserError("checkpoint number must be at least cp01")
    test_name = selection if task is not None else None
    return project_name, test_name


def test(selection: str, preset: str, *, reconfigure: bool, dry_run: bool,
         repeat: int) -> None:
    project_name, test_name = parse_selection(selection)
    build(project_name, preset, reconfigure=reconfigure, dry_run=dry_run)
    ctest = require_tool("ctest", dry_run=dry_run)

    command = [ctest, "--preset", preset]
    if test_name is None:
        command += ["-L", f"^{project_name}$"]
    else:
        command += ["-R", f"^{test_name}$"]
    if repeat > 1:
        command += ["--repeat", f"until-fail:{repeat}"]
    if preset == "ci-lavapipe":
        xvfb_run = require_tool("xvfb-run", dry_run=dry_run)
        command = [xvfb_run, "-a"] + command

    scope = selection if test_name is not None else f"all {project_name} tests"
    print(f"[test] {scope}")
    run_command(command, dry_run=dry_run)


def executable_path(project_name: str, preset: str) -> Path:
    project = PROJECTS[project_name]
    if project.app_target is None or project.executable_dir is None:
        raise UserError(f"{project_name} has no runnable application")
    suffix = ".exe" if preset.startswith("win-") else ""
    return ROOT / "build" / preset / project.executable_dir / f"{project.app_target}{suffix}"


def run_app(project_name: str, preset: str, *, reconfigure: bool, dry_run: bool,
            frames: Optional[int], stage: Optional[int]) -> None:
    project = PROJECTS[project_name]
    if frames is not None and not project.accepts_frames:
        raise UserError(f"{project_name} does not understand --frames")
    if stage is not None:
        if project.stages == 0:
            raise UserError(f"{project_name} does not take a stage number")
        if stage > project.stages:
            raise UserError(
                f"{project_name} stage must be between 1 and {project.stages}"
            )

    build(project_name, preset, reconfigure=reconfigure, dry_run=dry_run, app=True)
    command = [str(executable_path(project_name, preset))]
    if frames is not None:
        command += ["--frames", str(frames)]
    if stage is not None:
        # p02_resources reads the stage as a bare positional argument.
        command.append(str(stage))
    print(f"[run] {project_name}")
    run_command(command, dry_run=dry_run)


def positive_int(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return number


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Configure, build, test, and run render-workbook without spelling out "
            "platform-specific CMake presets."
        )
    )
    parser.add_argument(
        "--preset",
        choices=tuple(SYSTEM_PRESETS.values()),
        help="override automatic host detection",
    )
    parser.add_argument(
        "--reconfigure",
        action="store_true",
        help="run the CMake configure step even when a cache already exists",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print commands without executing them",
    )

    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("doctor", help="show the selected preset and tool availability")
    commands.add_parser("configure", help="generate the platform build directory")

    build_parser = commands.add_parser("build", help="compile one project test target")
    build_parser.add_argument("project", choices=tuple(PROJECTS), nargs="?", default="p01")

    test_parser = commands.add_parser(
        "test", help="build and test a whole project, one pXX-tNN task, or a checkpoint"
    )
    test_parser.add_argument("selection", help="for example: p02, p02-t04, or p02-t04-cp01")
    test_parser.add_argument(
        "--repeat",
        type=positive_int,
        default=1,
        metavar="N",
        help="repeat each selected test until failure, at most N times",
    )

    run_parser = commands.add_parser("run", help="build and run a project application")
    run_parser.add_argument("project", choices=tuple(PROJECTS), nargs="?", default="p01")
    run_parser.add_argument("--frames", type=positive_int,
                            help="p01 only: exit after this many frames")
    run_parser.add_argument("--stage", type=positive_int, metavar="N",
                            help="p02 only: initialise up to stage N (1-8) instead of the last one")
    return parser


def doctor(preset: str, *, dry_run: bool) -> None:
    print(f"host:    {platform.system()} / {platform.machine()}")
    print(f"preset:  {preset}")
    print(f"{'python:':8} {sys.executable}")
    for tool in ("cmake", "ctest", "ninja"):
        print(f"{tool + ':':8} {find_tool(tool) or 'NOT FOUND'}")
    if preset == "win-msvc":
        print(f"{'cl:':8} {shutil.which('cl') or 'NOT FOUND (open an MSVC tools terminal)'}")
    if dry_run:
        print("dry-run: enabled")


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = make_parser().parse_args(argv)
    try:
        preset = args.preset or detect_preset()
        if args.command == "doctor":
            doctor(preset, dry_run=args.dry_run)
        elif args.command == "configure":
            configure(preset, reconfigure=args.reconfigure, dry_run=args.dry_run)
        elif args.command == "build":
            build(args.project, preset, reconfigure=args.reconfigure, dry_run=args.dry_run)
        elif args.command == "test":
            test(
                args.selection,
                preset,
                reconfigure=args.reconfigure,
                dry_run=args.dry_run,
                repeat=args.repeat,
            )
        elif args.command == "run":
            run_app(
                args.project,
                preset,
                reconfigure=args.reconfigure,
                dry_run=args.dry_run,
                frames=args.frames,
                stage=args.stage,
            )
        else:  # argparse keeps this unreachable, but makes the dispatch exhaustive.
            raise UserError(f"unknown command: {args.command}")
    except UserError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as exc:
        return exc.returncode or 1
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
