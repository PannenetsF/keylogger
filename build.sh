#! /bin/bash

clang++ -std=c++11 -framework Carbon -framework Cocoa keylogger.cpp -o keylogger
