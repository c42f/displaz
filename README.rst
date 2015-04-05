==============================================
displaz - A viewer for geospatial point clouds
==============================================

**displaz** is a cross platform viewer for displaying lidar point clouds and
derived artifacts such as fitted meshes.  The interface was originally
developed for viewing large airborne laser scans, but also works quite well for
point clouds acquired using terrestrial lidar and other sources such as
bathymetric sonar.

The goal is to provide a fully programmable technical tool for exploring lidar
point data sets and derived geometry.  Point cloud display can be customized
using OpenGL shaders connected to user defined GUI controls.  The displaz
interface itself can be scripted via a socket interface.

See `the user guide <doc/userguide.rst>`_ for usage examples and instructions.


Third party libraries used in displaz
-------------------------------------

Behind the scenes displaz uses code written by many people.  The following
third party projects are gratefully acknowledged:

* Qt - http://qt-project.org
* LASLib - http://www.cs.unc.edu/~isenburg/lastools
* ilmbase - http://www.openexr.com
* rply - http://www.impa.br/~diego/software/rply
* GLEW - http://glew.sourceforge.net/
* Small pieces from OpenImageIO - http://openimageio.org


Supported Systems
-----------------

displaz is regularly compiled on linux and windows.  It has also been compiled
on OSX but doesn't yet work properly on all versions.

displaz is known to work well with recent NVidia and ATI graphics cards and
drivers.  Some issues have been observed with intel integrated graphics and
older ATI drivers.


Troubleshooting
~~~~~~~~~~~~~~~

Q: I see weird rendering artifacts.  Points which are in the background
sometimes hide those in the foreground.

A: Some graphics cards (for example integrated Intel cards on many laptops)
seem to work badly with displaz.  First, ensure that your graphics drivers are
up to date.  For laptops which have a higher power discrete GPU (such as from
NVidia or ATI), you should ensure that you're actually using it - many systems
also have a low power Intel GPU which is used by default on windows.  


Building
--------

Before building, you need the following

* cmake >= 2.8
* Qt >= 4.7 (however Qt 5.0 and greater is not supported yet)
* GLEW >= 1.5.2 (or so)
* Python docutils (optional - required to build the html documentation)

The additional third party libraries are downloaded and built automatically by
the build system (LASlib and IlmBase), or included with the displaz source
code if they're small enough.

