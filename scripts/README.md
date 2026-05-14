
This directory contains all the scripts that were used to build, test, measure
performance etc. pocketlang. These scripts in this directory are path
independent -- can be run from anywhere.

## Build related scripts

`amalgamate.py` - generates a single-header distribution from current sources.

```bash
python3 scripts/amalgamate.py > /tmp/pocketlang.h
```

## Running Benchmarks

Build a release version of pocketlang with CMake, then run the following
command. It'll generate a benchmark report named `report.html` in this
directory.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

```
python3 run_benchmarks.py
```

## Other Scripts

`generate_native.py` - will generate the native interface (C extension
for pocketlang) source files from the `pocketlang.h` header. Rest of
the scripts name speak for it self. If you want to add a build script
or found a bug, feel free to open an issue or a PR.
 
