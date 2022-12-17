# Small script to test if the examples still compile after compiler changes

from __future__ import annotations

from typing import Iterable, List, Any, Tuple, Union, TypedDict

import os
import pathlib
import subprocess
import shlex
import json

examples = pathlib.Path(__file__).parent / 'examples'
cwd = pathlib.Path(__file__).parent
if not (cwd / 'quart').exists():
    print('Quart executable not found. Please run "make" before running this script.')
    exit(1)

def run(executable: Union[str, os.PathLike[str]], args: Iterable[Any]) -> Tuple[int, str, str]:
    new: List[str] = [shlex.quote(str(arg)) for arg in args]
    new.insert(0, str(executable))

    process = subprocess.Popen(new, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd)
    process.wait()

    assert process.stdout and process.stderr
    return process.returncode, process.stdout.read().decode(), process.stderr.read().decode()

class ExampleResult(TypedDict):
    returncode: int
    args: List[str]
    stdout: str
    stderr: str

class Example:
    def __init__(self, file: pathlib.Path[str]) -> None:
        self.file = file

    def compile(self) -> None:
        returncode, stdout, stderr = run('./quart', [self.file])
        if returncode != 0:
            print(stdout)
            print(stderr)

            print('-----------------------------------------')
            print(f'Failed to compile: {file}. Update the example of fix the compiler.')

            exit(returncode)        

    def run(self) -> None:
        self.compile()
        
        result = self.parse_output_file()
        returncode, stdout, stderr = run(self.file.with_suffix(''), result['args'])

        if returncode != result['returncode']:
            print(f'Example {self.file} failed with return code {returncode}.')
            print('Expected return code:', result['returncode'])

            exit(1)

        if stdout != result['stdout']:
            print(f'Example {self.file} failed.')
            print('Expected stdout:', result['stdout'])
            print('Actual stdout:', stdout)

            exit(1)

        if stderr != result['stderr']:
            print(f'Example {self.file} failed.')
            print('Expected stderr:', result['stderr'])
            print('Actual stderr:', stderr)

            exit(1)

    def update(self, *args: str) -> None:
        self.compile()
        returncode, stdout, stderr = run(self.file.with_suffix(''), args)

        self.update_output_file(returncode, list(args), stdout, stderr)

    def has_output_file(self) -> bool:
        return self.file.with_suffix('.output.json').exists()

    def parse_output_file(self) -> ExampleResult:
        output = self.file.with_suffix('.output.json')
        with open(output, 'r') as f:
            return json.load(f)

    def update_output_file(
        self, returncode: int, args: List[str], stdout: str, stderr: str
    ) -> None:
        output = self.file.with_suffix('.output.json')
        with open(output, 'w') as f:
            json.dump({
                'returncode': returncode,
                'args': args,
                'stdout': stdout,
                'stderr': stderr
            }, f, indent=4)

i = 0
for file in examples.iterdir():
    if file.suffix != '.qr':
        continue

    example = Example(file)
    print(f'Running example {i} ({str(file)!r})')

    if not example.has_output_file():
        example.update()
        continue

    example.run()
    i += 1

print(f'\nSuccessfully compiled and tested {i} examples.')
