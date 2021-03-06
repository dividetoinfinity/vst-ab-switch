#!/usr/bin/env bash

# from http://stackoverflow.com/questions/59895/can-a-bash-script-tell-what-directory-its-stored-in
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
  DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
BASEDIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

mkdir -p build/Release
cd build/Release

if [ -n "${VST3_SDK_ROOT}" ]; then
  DVST3_SDK_ROOT="-DVST3_SDK_ROOT=${VST3_SDK_ROOT}"
fi

cmake ${DVST3_SDK_ROOT} -DCMAKE_BUILD_TYPE=Release ${BASEDIR}

# builds and runs the test
cmake --build . --target VST_AB_Switch_test
ctest

# builds the archive
cmake --build . --target archive
