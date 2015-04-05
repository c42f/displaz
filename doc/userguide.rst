Displaz user guide
==================

Quickstart
----------

Mouse controls

* Click and drag the left mouse button to rotate
* Click the middle mouse button to centre the camera on the closest point
  under the mouse
* Click and drag the right mouse button to zoom, or use the mouse wheel

Point display
~~~~~~~~~~~~~
To change how the points are displayed, use the controls in the box on
the right hand side of the screen.  Examples:

* To change the point radius, click and **vertically drag** on the arrows
  of the spinbox labeled "Point Radius".  This extension works for all
  spinboxes containing real numbers; somewhat nonstandard but extremely
  convenient.
* To view in colour, select "Colour" from the Colour Mode drop down box.

Shader editing
~~~~~~~~~~~~~~
Additional point display controls may be added and configured at runtime using
hints in the shader program.  While in the shader editor (accessible from the
view menu), pressing Shift+Return will cause a valid shader to be recompiled
and applied to the point cloud immediately.

TODO: Document custom GUI control hints

float
* scaling = [ exponential | linear ]
* speed = <float>
* min = <float>
* max = <float>

integer
* enum = <Name1> | <Name2> | <Name3>
* min = <int>
* max = <int>


TODO: Document shader conventions (vertex/fragment combination)


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
