# Small script to test if the examples still compile after compiler changes

from typing import List, Any
import pathlib
import subprocess
import shlex

examples = pathlib.Path(__file__).parent / 'examples'
cwd = pathlib.Path(__file__).parent

if not (cwd / 'proton').exists():
    print('Proton executable not found. Please run "make" before running this script.')
    exit(1)

def run(executable: str, args: List[Any]) -> int:
    new: List[str] = [shlex.quote(str(arg)) for arg in args]
    new.insert(0, shlex.quote(executable))

    process = subprocess.Popen(new)
    process.wait()

    return process.returncode

i = 0
for file in examples.iterdir():
    if file.suffix != '.pr':
        continue

    code = run('./proton', [file, '-c', '-o', file.with_suffix('.o')])
    if code != 0:
        print('-----------------------------------------')
        print(f'Failed to compile: {file}. Update the example of fix the compiler.')

        exit(code)

    i += 1

print(f'Successfully compiled {i} examples.')
