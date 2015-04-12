==========
User guide
==========

Navigation
----------

Displaz uses three mouse buttons to control the 3D view:

* *Rotate* the view by dragging with the left mouse button
* *Centre* the camera on the closest point under the mouse by clicking the middle mouse button
* *Zoom* by dragging with the right mouse button (or with the mouse wheel)

To change how points are displayed, use the controls in the *Shader Parameters*
box on the right hand side of the screen.  For example, to change the point
radius, click and *vertically drag* the arrows of the spinbox labelled "Point
Radius" (yes, this is nonstandard; it is also very useful).  To view any colour
channel present, select "Colour" from the "Colour Mode" drop down box.

Various information is available in the *Log* box on the bottom right,
including attributes of selected points, file load errors, and so forth.

The *Data Sets* box shows a list of all currently loaded data sets, allowing
the visibility to be controlled.  A selected data set may be unloaded by
pressing the delete key.


Shader editing
--------------

Displaz allows full control over how points are displayed in the 3D window by
allowing the user to edit and recompile the shader program which is accessible
via *View->Shader Editor*.  Shaders are written in the OpenGL shading language
with some extra hints in GLSL comments describing how to connect shader
variables to the controls in the *Shader Parameters* dialog box.  After editing
a shader, pressing *Shift+Return* will cause the shader to be recompiled and
the 3D view updated.

As a fairly minimal example, consider the file
`simple_example.glsl <../shaders/simple_example.glsl>`_
from the shaders directory.

.. include:: ../shaders/simple_example.glsl
  :literal:

Each shader source file consists of both a vertex and fragment shader,
distinguished by use of the ``VERTEX_SHADER`` and ``FRAGMENT_SHADER`` macros.

At the least, the vertex shader should perform a perspective transformation to
set the built in ``gl_Position`` and ``gl_PointSize`` variables as shown.  If
you're interested in a nontrivial colour, you should also set a colour output
variable which will need to match a colour input variable in the fragment shader.

The fragment shader must set a per pixel colour - in this case it is inherited
very simply from the vertex shader.  It can also decide to discard fragments so
that point shapes other than squares can be created (see for example the
default shader ``las_points.glsl`` in the shaders directory.)


Shader parameter GUI controls
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To make it easy to quickly modify the shader uniform parameters, these can be
annotated on the same line with a special comment beginning with ``//#`` of the
general form::

  uniform <type> varname = <default>; //#key1=value1; [ key2=value2; ... ]

The key value pairs define the behaviour and appearance of the corresponding
GUI control.  Uniform float parameters will be rendered as a spin box.
Configuration keys are:

``//# scaling = [ exp | linear ]``
  Define how the value scales as the mouse is dragged.  Exponential scaling is
  good for magnitudes like the point radius.  Linear scaling is more
  appropriate for .  Default: exponential.
``//# speed = <float>``
  Rate at which the value scales as the mouse is dragged.  Default 1.
``//# min = <float>; max = <float>``
  Define minimum and maximum possible values for the parameter.  Defaults 0,100.

Uniform integer parameters may be rendered as a spin box or combo box:

``//# enum = <Name1> | <Name2> | <Name3>``
  Display a combo box with the given names as choices.  The shader will be
  given the index of the currently selected element of the box, starting at
  zero.  Default: none.
``//# min = <int>; max = <int>``
  Allowed range for integer values in spin box.  Defaults 0,100.


File formats
------------

Displaz can load the following point cloud types:

:las: Point clouds in the ASPRS laser scan exchange format
:laz: Compressed las files using Martin Isenberg's laszip format
:ply: Point clouds in the Stanford triangle format containing the
      ``position.{x,y,z}`` or ``vertex_position.{x,y,z}`` properties as
      described below.
:txt: Simple plain text point clouds in space-separated XYZ format

Simple triangle and line meshes are also supported:

:ply: line segments files containing position and the ``edge.vertex_index`` property
:ply: triangle meshes containing position and the ``face.vertex_index`` property

Point clouds
~~~~~~~~~~~~

las and laz format
..................

The main file format is the ASPRS LASer format; this is particularly
appropriate for airborne laser scans.  Internally these formats are read using
the LASlib (by default), so any standards-conforming las file should be read
correctly.


ply format details
..................

The `ply format <http://paulbourke.net/dataformats/ply>`_ is a very flexible
format for storing vertices and vertex connectivity.  Due to its simplicity ply
was chosen as a general format to communicate point cloud, mesh and line
segment information to displaz.  Unfortunately the standard values for ply
metadata are poorly specified which leads to interoperability problems.  The
metadata conventions chosen in displaz are documented below.

Having all fields as a mix of properties on the ``vertex`` element seems to be
a common way to store point clouds in ply format.  Properties of the ``vertex``
element are loaded into displaz shader variables as follows::

  (x,y,z)          -> vec3 position
  (nx,ny,nz)       -> vec3 normal
  (red,green,blue) -> vec3 color
  (r,g,b)          -> vec3 color

Other attributes are processed as follows::

  prop[0], ..., prop[N]   -> N-element array per point with name "prop"
  propx, propy, propz     -> 3-element vector per point with name "prop"
  prop_x, prop_y, prop_z  -> 3-element vector per point with name "prop"


Here is a complete header structure in this format from the file
``standard_ply_points.ply``.  In it we define variables ``position``,
``normal``, ``color`` and a two element array ``a``::

  ply
  format ascii 1.0
  comment Point cloud test containing some standard ply field names + extras
  element vertex 20
  property float x
  property float y
  property float z
  property float nx
  property float ny
  property float nz
  property uint8 red
  property uint8 green
  property uint8 blue
  property float a[0]
  property float a[1]
  end_header


displaz native ply point format
...............................

To get data into displaz from the various language bindings, a native
serialization format is under development.  The format a valid ply file, with
metadata designed to be unambiguous, and data layout simple and efficient for
binary serialization from in memory arrays.  Each ply element should be named
``vertex_<field_name>``; the ``vertex_`` prefix is stripped off by displaz
during parsing, with a resulting field ``<field_name>`` visible in the shader.
The semantics of the field are determined by the names of the first property:
"x" implies a vector, "r" a color, and "0" an array.

For example, the example file ``displaz_ply_points.ply`` has the header::

  ply
  format ascii 1.0
  element vertex_position 20
  property float x
  property float y
  property float z
  element vertex_mycolor 20
  property uint8 r
  property uint8 g
  property uint8 b
  element vertex_myarray 20
  property float 0
  property float 1
  end_header

will be parsed and made available to the shader in the form::

  vector float[3] position
  color uint8_t[3] mycolor
  array float[2] myarray


Triangle and line meshes
~~~~~~~~~~~~~~~~~~~~~~~~

Basic support for triangle and line meshes is available using what I hope is
the standard ply format for these.  For both triangles and lines, the array of
vertex positions are defined by properties ``vertex.x``, ``vertex.y`` and
``vertex.z``.  Triangles are defined by connecting three vertices by specifying
indices into the vertex array in the property list ``face.vertex_index``.
Similarly, edges are defined by connecting any number of vertices into a
polygonal line using the property list ``edge.vertex_index``.  For example, the
following header structure defines a mesh with 10 faces, and 3 polygonal lines::

  ply
  format ascii 1.0
  element vertex 20
  property float x
  property float y
  property float z
  element color 20
  property float r
  property float g
  property float b
  element face 10
  property list uchar int vertex_index
  element edge 3
  property list uchar int vertex_index
  end_header


Troubleshooting
---------------

**Q**: I see weird rendering artefacts.  Points which are in the background
sometimes hide those in the foreground.

**A**: Some graphics cards (for example integrated Intel cards on many laptops)
seem to work badly with displaz.  First, ensure that your graphics drivers are
up to date.  For laptops which have a higher power discrete GPU (such as from
NVidia or ATI), you should ensure that you're actually using it - many systems
also have a low power Intel GPU which is used by default on windows.


**Q**: I can't see any colour, even though my las file is meant to have colour
included.

**A**: Try setting exposure to 256.  If this works, you have a las file with
colours scaled between 0-255, whereas they should be scaled into the full range
of 0-65535 according to the las standard.


