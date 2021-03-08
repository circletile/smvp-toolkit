# smvp-toolkit
Implement sparse matrix vector product calculations using CSR and TJDS compression algorithms. This program is part of university coursework.

**IMPORTANT: CMake is required to build this program. CMake is available at https://cmake.org/download/**
**IMPORTANT: Libeary popt is required to build this program. Check your OS's package manager for libpopt-devel or similar.**

Build instructions:
```
git clone https://github.com/circletile/smvp-toolkit.git
cd smvp-csr
cmake --build ./build --config release --clean-first
```

Run instructions:
```
./build/smvp-toolkit-cli --all-algs -n 1000 /path/to/matrixmarket/file.mtx

For additional usage instructions:

./build/smvp-toolkit-cli --help
```
After a run completes, a report file will be generated in the current working directory.
