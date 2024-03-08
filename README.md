# simget

avr simulator toy


# clone it with submodules


# build it , after genereating the cmake's with build folders..

install cmake, vcpkg, and change the toolchain path if different

i use these aliases

alias cmake-d='cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=~/code/vcpkg/scripts/buildsystems/vcpkg.cmake  -DCMAKE_BUILD_TYPE=Debug'
alias cmake-r='cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=~/code/vcpkg/scripts/buildsystems/vcpkg.cmake  -DCMAKE_BUILD_TYPE=Release'

# short hand

cd simavr
make
cmake --build build -j 5
cd simavr
make
cmake --build build -j 5
cd ../../
cd avrdisas
cmake --build build -j 5
make
cd ..
cmake --build build -j 5

# run it

-f frequency
--mcu supported mcu

./build/simget --mcu attiny4313 -f 1000000 --firmware ./elliePOV.hex
#./build/simget --mcu attiny4313 -f 1000000 --firmware simavr/tests/attiny4313_port.hex
