========================================
Displaz - Yet another LiDAR point viewer
========================================

Displaz is a viewer application for displaying LiDAR points using qt and
OpenGL.  It currently supports the las and laz formats via laslib (see
http://www.cs.unc.edu/~isenburg/lastools/).


Building
--------

Displaz should build on both windows and linux using the cmake-based build
system.  It has quite a few dependencies at the moment:

* Qt4
* OpenGL
* LASlib (note: not the same as liblas!)
* IlmBase (will possibly remove this dependency)


Todo
----

* Indexing for faster rendering
* General point attributes
* Render modes
* Shader pipeline
* Shader editing
* Persistent settings
* Use liblas (?)
* Measurement tool
* Select point subset
* Investigate approximate ambient occlusion to render points
* Socket mode for updating point sets
* Multithreaded point cloud loading
