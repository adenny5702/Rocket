Configure cmake: `cmake -S . -B build -G Ninja -DBUILD_APP=ON -DBUILD_AIR=ON -DBUILD_GROUND=ON`

Build: `cmake --build build`

Install submodules: `git submodule update --init --recursive`