# smvp-toolkit
Implement sparse matrix vector product calculations using CSR and TJDS compression algorithms. This program is part of university coursework.

**IMPORTANT: CMake is required to build this program. CMake is available at https://cmake.org/download/**

Build instructions:
```
git clone https://github.com/circletile/smvp-toolkit.git
cd smvp-csr
cmake --build ./build --config release --clean-first
```

Run instructions:
```
./build/smvp-toolkit /path/to/matrixmarket/file.mtx
```
After a run completes, a report file will be generated in the current working directory.
