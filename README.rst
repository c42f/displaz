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
The following commands may be used to build displaz on linux::

    # Install dependencies (apt-based distributions)
    sudo apt-get install git g++ cmake libqt4-dev libqt4-opengl-dev python-docutils

    # Install dependencies (yum-based distributions)
    sudo yum install git gcc-c++ make patch cmake qt-devel python-docutils

    # Install dependencies (OpenSuse distributions)
    sudo zypper install git gcc-c++ libqt4-devel glu-devel python-docutils

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


Windows
~~~~~~~
To install the dependencies on windows, manually download and install the
following tools:

* `cmake <http://www.cmake.org/download/>`_
* `msysgit <https://msysgit.github.io/>`_

For a 64 bit windows compile, you'll need to download the
`Qt4 source code package <http://download.qt.io/archive/qt/4.8/4.8.6>`_
since there are no official 64 bit binary packages.  For MSVC, configure from
the **x64** complier command prompt using::

    configure -release -no-webkit -nomake demos -nomake examples -opensource -platform win32-msvc2012

The author builds displaz itself from the Visual Studio 2012 Express edition
x64 cross tools command prompt using nmake.  First, clone the repository using
the msysgit command line::

    # Get the source code
    git clone https://github.com/c42f/displaz.git

Next, in the x64 command prompt::

    rem Build LASlib and ilmbase
    cd displaz
    mkdir build_external
    cd build_external
    cmake -G "NMake Makefiles" ..\thirdparty\external
    nmake
    cd ..

    rem Build displaz.
    rem Assumes that Qt has been installed into C:\Qt\4.8.6
    mkdir build
    cd build
    cmake -G "NMake Makefiles" ^
        -D QT_QMAKE_EXECUTABLE:PATH=C:\Qt\4.8.6\bin\qmake.exe ^
        -D CMAKE_INSTALL_PREFIX:PATH=dist ^
        ..
    nmake

    rem Optionally, create the installer package
    nmake package


Generic build
~~~~~~~~~~~~~
To build displaz, install the following tools:

* cmake >= 2.8
* Python docutils (optional - required to build the html documentation)

Displaz also depends on several libraries.  For simplicity, the smaller
dependencies are bundled in the thirdparty directory.  There's also an
automated download/build system for some of the larger ones (LASlib and
ilmbase) available at ``thirdparty/external/CMakeLists.txt``.  However, you
will need to install the following manually:

* Qt >= 4.7 (Note that Qt 5.0 and greater is not supported yet)
* OpenGL >= 3.2
* ilmbase >= 1.0.1 (You don't need to install this if you're using the
  automated thirdparty build)

Both the LASlib and IlmBase libraries may be built using the separate third
party build system in ``thirdparty/external/CMakeLists.txt``.

Build options
~~~~~~~~~~~~~
To read the .las and .laz file formats, you'll need one of the following:

* LASlib >= something-recent (known to work with 150406).  This is the default
  because it's reasonably fast and has no additional library dependencies.
* PDAL >= something-recent (known to work with 0.1.0-3668-gff73c08).  You may
  select PDAL by setting the build option ``DISPLAZ_USE_PDAL=TRUE``.  Note that
  building PDAL also requires several libraries including boost, laszip and
  GDAL.

If you only want to read ply files (for example, to use the scripting language
bindings), and don't care about las you may set the build option
``DISPLAZ_USE_LAS=FALSE``.


Supported Systems
-----------------

displaz is regularly compiled on linux and windows.  It has also been compiled
on OSX but doesn't yet work properly on all versions.  displaz is known to work
well with recent NVidia and ATI graphics cards and drivers.  Some issues have
been observed with intel integrated graphics and older ATI drivers.


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

