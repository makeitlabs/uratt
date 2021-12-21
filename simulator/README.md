# uRATT LVGL UI Simulator

## Submodule Init

If you cloned the uRATT repo recursively, it should have pulled down the submodules already.  

If the `lvgl` and `lv_drivers` directories are empty, init the submodules manually.

    cd ~/uratt/simulator
    git submodule init
    git submodule update

## Install some pre-requisites

This is for Ubuntu.

    sudo apt-get update && sudo apt-get install -y build-essential libsdl2-dev

## Set up CMake build

    cd ~/uratt/simulator
    mkdir build
    cd build


## Build

From the `build` directory you just made above...

    cmake ..
    cmake --build . --parallel


## Run

From the `build` directory you made earlier.

    ../bin/main


## Build and conditionally run if successful

This is good for quick iteration changes.  Like the other instructions, run this from within the `build` directory.

    cmake --build . ; if [ $? -eq 0 ]; then ../bin/main; fi

