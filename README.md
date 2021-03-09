# smvp-toolkit
Implement sparse matrix vector product calculations using CSR and TJDS compression algorithms. This program is part of university coursework.

**IMPORTANT: CMake is required to build this program. CMake is available at https://cmake.org/download/**

**The popt development library is also required to build this program.**

Install popt on Debian/Ubuntu:
```
apt install libpopt-devel
```

Install popt on OSX (requires Homebrew):
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install popt
```


smvp-toolkit build instructions:
```
git clone https://github.com/circletile/smvp-toolkit.git
cd smvp-csr
cmake --build ./build --config release --clean-first
```

smvp-toolkit run instructions:
```
./build/smvp-toolkit-cli --all-algs -n 1000 /path/to/matrixmarket/file.mtx

For additional options/features:

./build/smvp-toolkit-cli --help
```
After a run completes, a report file will be generated in the current working directory.
