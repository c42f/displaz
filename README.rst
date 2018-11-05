==============================================
displaz - A viewer for geospatial point clouds
==============================================

**displaz** is a cross platform viewer for displaying lidar point clouds and
derived artifacts such as fitted meshes.  The interface was originally
developed for viewing large airborne laser scans, but also works quite well for
point clouds acquired using terrestrial lidar and other sources such as
bathymetric sonar.

The goal is to provide a flexible and programmable technical tool for exploring
large lidar point data sets and derived geometry.

* *Open point clouds up to the size of main memory.*  Performance remains
  interactive as the number of points becomes too large to draw in a single
  frame.
* *Create custom point visualizations.*  The OpenGL shader can be
  edited interactively.  In the shader program, you automatically have access
  to any per-point attributes defined in the input file.  Shader parameters are
  connected to user-defined GUI controls.
* *Plot interactively from your favourite programming language.*
  Displaz IPC lets you script the interface from the command line.
  Experimental language bindings are available for C++, python, julia and
  Matlab.


User guide
----------

See `the user guide <doc/userguide.rst>`_ for usage examples and instructions.


Installation
------------

Binary installer packages for windows are provided on the `releases page
<https://github.com/c42f/displaz/releases>`_.  For linux, it should be fairly
easy to build it yourself by following the instructions below.


Building
--------

Linux
~~~~~
Install dependencies using your package manager.  Here's a handy list of
dependencies for several distributions::

    # Ubuntu >= 14.04 (and probably other debian-based distributions)
    sudo apt-get install git g++ cmake qt5-default python-docutils

    # Mint
    sudo apt-get install git g++ cmake qt5-default libqt5opengl5-dev python-docutils

    # Older ubuntu (qt4 based - add cmake flag -DDISPLAZ_USE_QT4=TRUE)
    sudo apt-get install git g++ cmake libqt4-dev libqt4-opengl-dev python-docutils

    # Fedora 28
    sudo yum install git gcc-c++ make patch cmake qt5-qtbase-devel mesa-libGLU-devel python-docutils

    # OpenSuse
    sudo zypper install git gcc-c++ libqt5-qtbase-devel glu-devel python-docutils

The following commands may be used to build displaz on linux::

    # Get the source code
    git clone https://github.com/c42f/displaz.git
    cd displaz

    # Build LASlib and ilmbase
    mkdir build_external
    cd build_external
    cmake ../thirdparty/external
    make -j4
    cd ..

    # Build displaz
    mkdir build
    cd build
    cmake ..
    make -j4

    # Install into CMAKE_INSTALL_PREFIX=/usr/local
    sudo make install


Troubleshooting:

* Some people have had issues with a version of qt in their path clashing with
  the qt headers installed on the system. This may give an error such as
  "undefined reference to qt_version_tag", or some other qt library-related
  link error.  For example having the qt version distributed with the python
  package system ``conda`` has been known to cause issues, which can be solved
  by removing it from the ``$PATH`` variable *before* calling cmake in the
  script above.


Windows x64
~~~~~~~~~~~
The windows releases are built using cmake and
`Visual Studio <https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx>`_.
To install the dependencies on windows, manually download and install the
following tools:

* `cmake <http://www.cmake.org/download/>`_
* `msysgit <https://msysgit.github.io/>`_
* `qt5 <http://www.qt.io/download-open-source>`_ (ensure you get the correct version for your compiler)
* `nsis <http://nsis.sourceforge.net/Download>`_ (only required for installable package creation)

To build, first clone the repository using the msysgit command line::

    # Get the source code
    git clone https://github.com/c42f/displaz.git

You can build displaz with various supported cmake build system generators.
For the continuous integration build (and probably future releases), the Visual
Studio generator ``"Visual Studio 14 Win64"`` is used::

    rem Build LASlib and ilmbase
    mkdir build_external
    cd build_external
    cmake -G "Visual Studio 14 Win64" -D CMAKE_BUILD_TYPE=Release ..\thirdparty\external
    cmake --build . --config Release
    cd ..

    rem Build displaz.
    rem Assumes that Qt has been installed into C:\Qt\Qt5.5.1\5.5\msvc2015_64
    mkdir build
    cd build
    cmake -G "Visual Studio 14 Win64" ^
        -D CMAKE_PREFIX_PATH=C:\Qt\Qt5.5.1\5.5\msvc2015_64 ^
        -D CMAKE_INSTALL_PREFIX:PATH=dist ^
        ..
    cmake --build . --config Release

    rem Optionally, create the installer package
    cmake --build . --config Release --target package

Some of the cmake generators such as ``NMake Makefiles"`` won't find visual
studio unless it's in the path.  In that case you'd need to launch the steps
above from the x64 cross tools command prompt.


OSX
~~~

TODO - for the moment see the generic build instructions below.  Also note that
displaz is regularly built on OSX via travis-CI, so the commands in the file
``.travis.yml`` in the repository should more or less work.


Generic build
~~~~~~~~~~~~~
To build displaz, install the following tools:

* cmake >= 2.8.8
* Python docutils (optional - required to build the html documentation)

Displaz also depends on several libraries.  For simplicity, the smaller
dependencies are bundled in the thirdparty directory.  There's also an
automated download/build system for some of the larger ones (LASlib and
ilmbase) available at ``thirdparty/external/CMakeLists.txt``.  However, you
will need to install the following manually:

* Qt >= 5.0  (qt-4.8 is still semi-supported on linux)
* OpenGL >= 3.2
* ilmbase >= 1.0.1 (You don't need to install this if you're using the
  automated thirdparty build)

Both the LASlib and IlmBase libraries may be built using the separate third
party build system in ``thirdparty/external/CMakeLists.txt``.


Supported Systems
-----------------

displaz is regularly compiled on linux, OSX and windows.  It's known to work
well with recent NVidia and ATI graphics cards and drivers.  Some issues have
been observed with Intel integrated graphics and older ATI drivers.  If you
observe rendering artifacts there's a reasonable chance that your graphics card
or drivers are playing dirty tricks


Third party libraries used in displaz
-------------------------------------

Behind the scenes displaz uses code written by many people.  The following
third party projects are gratefully acknowledged:

* Qt - http://qt-project.org
* LASLib - http://www.cs.unc.edu/~isenburg/lastools
* PDAL - http://www.pdal.io
* ilmbase - http://www.openexr.com
* rply - http://www.impa.br/~diego/software/rply
* GLEW - http://glew.sourceforge.net/
* Small pieces from OpenImageIO - http://openimageio.org

