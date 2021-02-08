# smvp-csr
Implement sparse matrix vector product using CSR format as part of university coursework.

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
