#!/bin/sh
ori_pwd=${PWD}

function hack_clear_base()
{
    rm Makefile -rf
    rm cmake_install.cmake -rf
    rm CMakeCache.txt -rf
    rm CMakeFiles/ -rf
}

function hack_clear_all()
{
    hack_clear_base
}

# for release
rm -rf release
mkdir -p release/bin
mkdir -p release/modules

# for hack module
cd ${ori_pwd}/modules/
make cleanall
make
mv do_execve.ko ${ori_pwd}/release/modules/
make clean

# for hack app
cd ${ori_pwd}/src/
hack_clear_all
cmake CMakeLists.txt && make
mv hack_monitor ${ori_pwd}/release/bin/
mv hack_server ${ori_pwd}/release/bin/
hack_clear_base
