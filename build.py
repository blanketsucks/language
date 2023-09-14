from __future__ import annotations

from typing import List, Optional, NamedTuple

import os
from pathlib import Path
import argparse
import subprocess
import threading

ROOT_DIRECTORY = Path(__file__).parent

QUART_LIB_PATH = ROOT_DIRECTORY / 'lib'

DEFAULT_SOURCE_DIRECTORY = ROOT_DIRECTORY / 'src'
DEFAULT_BUILD_DIRECTORY = ROOT_DIRECTORY / 'build'

def _chunk(items: List[BuildStep], size: int) -> List[List[BuildStep]]:
    return [items[i:i + size] for i in range(0, len(items), size)]

def _replace_extension(filename: str, extension: str) -> str:
    return filename[:filename.rfind('.')] + extension

def create_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '--cxx',
        default=os.getenv('CXX', 'g++'),
        help='C++ compiler to use'
    )

    parser.add_argument(
        '--llvm-config',
        default='llvm-config-14',
        help='llvm-config program to use. Note that this must be LLVM 14.'
    )

    parser.add_argument(
        '--cxxflags',
        default='',
        help='A comma separated list of additional C++ flags to pass to the compiler'
    )

    parser.add_argument(
        '--ldflags',
        default='',
        help='A comma separated list of additional linker flags to pass to the compiler'
    )

    parser.add_argument(
        '--source-directory', '-S',
        default='src',
        help='Directory containing source files. Defaults to `src`'
    )

    parser.add_argument(
        '--build-directory', '-B',
        default='build',
        help='Directory containing build files. Defaults to `build`'
    )

    parser.add_argument(
        '--include-directory', '-I',
        default='include',
        help='Directory containing include files for the project. Defaults to `include`'
    )

    parser.add_argument(
        '--output', '-o',
        default='quart',
        help='Name of the output file. Defaults to `quart`'
    )

    parser.add_argument(
        '--jobs', '-j',
        type=int,
        default=1,
        help='Number of jobs to run in parallel'
    )

    parser.add_argument('--release', action='store_true', help='Build in release mode', default=False)
    parser.add_argument('--clean', action='store_true', help='Clean the build directory', default=False)
    parser.add_argument('--verbose', action='store_true', help='Print verbose output', default=False)

    return parser

def find_cxx_compiler() -> str:
    cxx = os.getenv('CXX', 'g++')
    try:
        process = subprocess.run([cxx, '--version'], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    except FileNotFoundError:
        raise RuntimeError(f'Could not find C++ compiler {cxx!r}') from None
    
    if process.returncode != 0:
        raise RuntimeError(f'Could not find C++ compiler {cxx!r}') from None

    return cxx

def try_llvm_config(config: str) -> Optional[str]:
    try:
        process = subprocess.run([config, '--version'], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    except FileNotFoundError:
        raise RuntimeError(f'Could not find {config!r}') from None
    
    if process.returncode != 0:
        raise RuntimeError(f'Could not find llvm-config {config!r}') from None

    major, _, _ = process.stdout.decode().strip().split('.')
    if int(major) != 14:
        raise RuntimeError(f'Expected LLVM 14, got LLVM {major}')
    
    return config

def get_llvm_libraries(config: str) -> List[str]:
    process = subprocess.run([config, '--libs', '--system-libs', '--ldflags'], stdout=subprocess.PIPE)
    return process.stdout.decode().strip().split()

def get_llvm_cxxflags(config: str) -> List[str]:
    process = subprocess.run([config, '--cxxflags'], stdout=subprocess.PIPE)
    return process.stdout.decode().strip().split()

COMPILER = find_cxx_compiler()

class BuildStep:
    def __init__(self, filename: str, *cxxflags: str, build: Build) -> None:
        self._build = build

        self.file = Path(filename).resolve()
        if build.source_directory:
            self.filename = str(self.file.relative_to(build.source_directory))
        else:
            self.filename = filename

        self.cxxflags = list(cxxflags)
        self.has_error = False

    def __repr__(self) -> str:
        return f'<BuildStep filename={self.filename!r}>'
    
    def get_output_filename(self) -> Path:
        path = self._build.build_directory / _replace_extension(self.filename, '.o')
        path.parent.mkdir(exist_ok=True, parents=True)

        return path
    
    def should_recompile(self) -> bool:
        out = self.get_output_filename()
        if not out.exists():
            return True

        return os.path.getmtime(self.file) > os.path.getmtime(out)
    
    def get_compile_command(self, out: str) -> List[str]:
        return [COMPILER, *self.cxxflags, '-c', str(self.file), '-o', out]
        
    def run(self) -> str:
        out = str(self.get_output_filename())
        if not self.should_recompile():
            return out
        
        command = self.get_compile_command(str(out))
        print(f' - Building file {self.filename!r}')

        if self._build.verbose:
            fmt = ' '.join(command)
            print(f'    - Running Command {fmt!r}')

        try:
            process = subprocess.run(command)
        except Exception:
            print(f' - Failed to build {self.filename!r}')
            self.has_error = True

            return out
        
        if process.returncode != 0:
            print(f' - Failed to build {self.filename!r}')
            self.has_error = True

            return out
        
        return out

class LDFlags(NamedTuple):
    flags: List[str]
    libraries: List[str]

class Build:
    def __init__(
        self,
        *,
        executable: str,
        build_directory: Path,
        cxxflags: Optional[List[str]] = None,
        ldflags: Optional[LDFlags] = None,
        source_directory: Optional[Path] = None,
        include_directory: Optional[Path] = None,
        debug: bool = True,
        jobs: int = 1,
        verbose: bool = False
    ):
        self.executable = executable
        self.jobs = jobs
        self.verbose = verbose

        build_directory.mkdir(exist_ok=True, parents=True) 

        self.source_directory = source_directory
        self.include_directory = include_directory
        self.build_directory = build_directory

        self.cxxflags = cxxflags or []
        if self.include_directory:
            self.cxxflags.append(f'-I{self.include_directory}')

        self.ldflags = ldflags or LDFlags([], [])
        if debug:
            self.cxxflags.append('-g')
            self.ldflags.flags.append('-g')

        self.steps: List[BuildStep] = []

    def add_flag(self, flag: str, value: Optional[str] = None) -> None:
        if value:
            self.cxxflags.append(f'{flag}{value}')
        else:
            self.cxxflags.append(flag)

    def add_steps_from_source(self) -> None:
        if not self.source_directory:
            return None

        [self.add_step(str(path)) for path in self.source_directory.glob('**/*.cpp')]

    def add_step(self, filename: str, *cxxflags: str) -> None:
        step = BuildStep(
            filename, 
            *self.cxxflags,
            *cxxflags,
            build=self
        )

        self.steps.append(step)

    def get_linker_command(self, files: List[str]) -> List[str]:
        out = self.build_directory / self.executable
        return [COMPILER, *self.ldflags.flags, *files, '-o', str(out), *self.ldflags.libraries]

    def link(self, files: List[str]) -> None:
        print(f' - Linking executable {self.executable!r}')
        command = self.get_linker_command(files)

        if self.verbose:
            fmt = ' '.join(command)
            print(f'    - Running Command {fmt!r}')

        subprocess.run(self.get_linker_command(files))

    def compile(self) -> Optional[List[str]]:
        files = []
        for step in self.steps:
            file = step.run()
            if step.has_error:
                return None

            files.append(file)

        return files
    
    def run(self) -> None:
        if self.jobs > 1:
            self._run_parallel()
        else:
            files = self.compile()
            if not files:
                return

            self.link(files)

    def _run_parallel(self) -> None:
        threads: List[threading.Thread] = []
        steps = [step for step in self.steps if step.should_recompile()]

        for chunk in _chunk(steps, self.jobs):
            for step in chunk:
                thread = threading.Thread(target=step.run)
                thread.start()

                threads.append(thread)

            for thread in threads:
                thread.join()

            threads.clear()

        if any(step.has_error for step in self.steps):
            return None

        self.link([str(step.get_output_filename()) for step in self.steps])

def main():
    parser = create_argument_parser()
    args = parser.parse_args()

    try_llvm_config(args.llvm_config)

    source_directory: Path = ROOT_DIRECTORY / args.source_directory
    build_directory: Path = ROOT_DIRECTORY / args.build_directory
    include_directory: Path = ROOT_DIRECTORY / args.include_directory

    if args.clean:
        for file in build_directory.glob('**/*'):
            if file.is_file(): 
                file.unlink()

        def _rmdir(directory: Path) -> None:
            for file in directory.glob('**/*'):
                if file.is_dir():
                    _rmdir(file)
                else:
                    file.unlink()
            
            directory.rmdir()

        for file in build_directory.glob('**/*'):
            if file.is_dir():
                _rmdir(file)
                

        return None

    cxxflags = [
        '-Wall', '-Wextra', '-O3',
        '-Wno-redundant-move',
        '-Wno-unused-variable',
        '-Wno-reorder',
        '-Wno-switch',
        '-Wno-unused-parameter',
        '-Wno-non-pod-varargs'
    ]

    build = Build(
        executable='quart',
        build_directory=build_directory,
        source_directory=source_directory,
        include_directory=include_directory,
        cxxflags=[*cxxflags, *get_llvm_cxxflags(args.llvm_config)],
        ldflags=LDFlags(
            flags=['-O3'],
            libraries=get_llvm_libraries(args.llvm_config)
        ),
        debug=not args.release,
        jobs=args.jobs,
        verbose=args.verbose
    )
    
    build.add_flag('-D', f'QUART_PATH="{QUART_LIB_PATH}"')
    build.add_steps_from_source()

    build.run()

if __name__ == '__main__':
    main()