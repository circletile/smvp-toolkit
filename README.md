# smvp-csr
Implement a sparse matrix vector product in C using the CSR compression algorithm. This program is part of university coursework.

**IMPORTANT: CMake is required to build this program. CMake is available at https://cmake.org/download/**

Build instructions:
```
git clone https://github.com/circletile/smvp-csr.git
cd smvp-csr
cmake --build ./build --config release --clean-first
```

Run instructions:
```
./build/smvp-csr /path/to/matrixmarket/file.mtx
```
After a run completes, a report file will be generated in the current working directory.
