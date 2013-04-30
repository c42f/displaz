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



Todo
----

Various things which would be nice to do, in rough order of precedence

* Rendering

  * Frustum culling of tiles
  * Autotune simplification quality based on total points
  * Fix GLSL deprecation warnings for builtin variables
  * Octree point heirarchy for improved robustness
  * Threaded rendering for better responsiveness
  * Approximate ambient occlusion (?)

* GUI

  * Trimming based on line segment
  * Better panning support
  * Persistent settings
  * Measurement tool
  * Syntax highlighting in shader editor
  * Hide and wrap cursor while dragging camera
  * First person camera and control scheme

* Point IO

  * General point attributes
  * Socket mode for updating point sets
  * Flexible text file import
  * Threaded point cloud loading
  * Import points from .ply files

* Infrastructure

  * Make displaz easier to build

    * Fix header situation with IlmBase
    * Use laszip directly instead of LASlib if possible

  * GLEW for simpler OpenGL portability

