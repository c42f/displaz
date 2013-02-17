========================================
Displaz - Yet another LiDAR point viewer
========================================

Displaz is a viewer application for displaying LiDAR points using qt and
OpenGL.  It supports the .las and .laz formats via laslib.


Building
--------

Displaz should build on both windows and linux using the cmake-based build
system.  Before building, you need to install the following libraries:

* Qt 4.7
* OpenGL (requires glsl 1.3)
* LASlib see http://www.cs.unc.edu/~isenburg/lastools
  (note that this library is distinct from liblas!)
* IlmBase (will possibly remove this dependency)


Todo
----

* Rendering
  * Threaded rendering for better responsiveness
  * Investigate approximate ambient occlusion (?)
  * Autotune stochastic simplification quality based on total points
* GUI
  * Persistent settings
  * Measurement tool
  * Syntax highlighting in editor
  * Hide and wrap cursor while dragging camera
* Point IO
  * General point attributes
  * Option to use laszip directly (?)
  * Socket mode for updating point sets
  * Threaded point cloud loading

* Bugs
  ...
