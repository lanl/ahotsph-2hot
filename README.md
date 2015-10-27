2hot
====
The subdirectories of this directory contain the files need to build 2hot.

Downloading
-----------
To download the driver programs ensure that all submodules are initialized and
pulled. This can be done in two ways

First: When performing the initial clone, add the `--recursive` flags

    git clone --recursive ${REPOSITORY}

If the repository has already been cloned, simply execute the following from
the root directory of the repository

    git submodule update --init --recursive

Dependencies
--------
The dependencies will vary depending upon the configuration, but in all cases CMake will search your `PATH` for the required headers and libraries.

With respect to CLASS (the Cosmic Linear Anisotropy Solving System), the `PATH` will be searched but the user may also define the `HOT_CLASS` environmental variable to point toward where CLASS was unpacked and built, as per the Makefile-based 2hot build system.

Building
--------
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



Execution
---------
