# Astrophysical Hashed Oct Tree-based Smoothed Particle Hydrodynamics code submodule: '2hot'

## Description

This is the new version of Warren & Salmon's hashed oct tree code - 2hot. 

The subdirectories of this directory contain the files need to build 2hot.

## Downloading

To download the driver programs ensure that all submodules are initialized and
pulled. This can be done in two ways

First: When performing the initial clone, add the `--recursive` flags

    git clone --recursive ${REPOSITORY}

If the repository has already been cloned, simply execute the following from
the root directory of the repository

    git submodule update --init --recursive

## Dependencies

The dependencies will vary depending upon the configuration, but in all cases CMake will search your `PATH` for the required headers and libraries.

With respect to CLASS (the Cosmic Linear Anisotropy Solving System), the `PATH` will be searched but the user may also define the `HOT_CLASS` environmental variable to point toward where CLASS was unpacked and built, as per the Makefile-based 2hot build system.

## Building

To build, either use the existing Makefiles and select an architecture that
corresponds to the target machine, or use CMake.

To use CMake, from the root directory of the repository

    mkdir bld
    cd bld
    cmake ..
    make

This will build 2hot and any downloaded submodules with the default
configuration, which sets `PAROS` to `seq` and uses the hardware clock for
timing. To change settings, either examine `CMakeLists.txt` to see what settings
to pass in or use `ccmake` instead of `cmake` for an interactive build.


## Execution


## Contributing
tbd

## License and copyright


BSD 3-Clause License

Copyright (c) 2026, Los Alamos National Laboratory. O5196.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
