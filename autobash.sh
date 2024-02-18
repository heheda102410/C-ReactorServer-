set -x
rm -rf `pwd`/build/*
cmake -B build
cmake --build build