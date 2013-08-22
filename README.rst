============================================
displaz - A viewer for geographic LiDAR data
============================================

**displaz** is a cross platform viewer application for displaying LiDAR point
clouds and high level features deduced from such point clouds.  The interface
and internals are optimised for viewing airborne laser scans of geographic data
(see, for example http://www.liblas.org/samples/autzen-colorized-1.2-3.laz),
but should also work for medium sized point clouds taken from other sources.

displaz is intended to be a highly flexible technical tool for exploring LiDAR
data sets and higher level geometric features arising from them.  Modern OpenGL
is chosen for point rendering to put the power of the OpenGL shading language
in the hands of the user: Shaders can be edited in real time in the shader
editor to provide the most useful visualisation for the task at hand.


Usage quickstart
----------------

Use File->Open to open a las file - for example files, see
http://www.liblas.org/samples.  Note that displaz may appear unresponsive when
loading very large files - just wait for it and it should eventually finish.


Navigation
~~~~~~~~~~

* Click and drag the left mouse button to rotate
* Click the middle mouse button to centre the camera on the closest point
  under the mouse
* Click and drag the right mouse button to zoom, or use the mouse wheel

Point display
~~~~~~~~~~~~~

To change how the points are displayed, use the controls in the box on
the right hand side of the screen.  Additional controls may be added and
configured at runtime via hints in the the shader program. Examples:

* To change the point radius, click and **vertically drag** on the arrows
  of the spinbox labeled "Point Radius".  This extension works for all
  spinboxes containing real numbers; somewhat nonstandard but extremely
  convenient.
* To view in colour, select "Colour" from the Colour Mode drop down box.


Supported file formats
----------------------

Point cloud input
~~~~~~~~~~~~~~~~~
The primary file format is the ASPRS .las format; compressed .laz is also
supported.  Internally these formats are read using the laszip library (via
LASlib) so any standards-conforming las file should be read correctly.

Very basic plain text import is also supported: Each line of a .txt file will
be interpreted as ``X Y Z ...`` where the trailing part of the line is ignored.

Vector geometry
~~~~~~~~~~~~~~~
Basic support for vector geometry (meshes and line segments) is available via
the .ply file format.  Any .ply file containing a header of the form::

    element vertex <N>
    property float x
    property float y
    property float z
    element face <M>
    property list uchar int vertex_index

will be recognised and rendered as a mesh with solid faces.  When the face
element is replaced with an edge element in the same format, each edge will be
rendered as a set of linear segments.


Supported Operating Systems
---------------------------

displaz is intended to be cross-platform, and is known to run on both windows
and linux.  The author doesn't have access to an OSX system, but welcomes
contributions toward making things work there as well.

The intention is to use modern OpenGL for all rendering tasks.  This gives us a
lot of power via the OpenGL shading language, but unfortunately presents
potential portability issues.  displaz is known to work well with NVidia
graphics cards; portability problems to other systems will be addressed as they
come up.


Building
--------

Before building, you need the following

* cmake >= 2.8
* Qt >= 4.7
* LASlib >= 120124 (NOTE: not the same as liblas from http://www.liblas.org!)
* IlmBase >= 1.0.2
* A **recent graphics card driver is required**, supporting at least OpenGL
  version 3.2.

Unfortunately LASlib (see http://www.cs.unc.edu/~isenburg/lastools) appears to
come without a build system - for now, the author recommends you simply build
all the source files together into a static library.


