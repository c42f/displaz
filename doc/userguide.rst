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


Supported file formats
----------------------

Point cloud input
~~~~~~~~~~~~~~~~~
The primary file format is the ASPRS .las format; compressed .laz is also
supported.  Internally these formats are read using the laszip library (via
LASlib) so any standards-conforming las file should be read correctly.

Very basic plain text import is also supported: Each line of a .txt file will
be interpreted as ``X Y Z ...`` where the trailing part of the line is ignored.

TODO: Document special displaz .ply format for points


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

TODO: Examples of

* ply mesh
* ply lines
* "standard" ply points



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


