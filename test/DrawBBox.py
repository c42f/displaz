#!/usr/bin/env python2
import subprocess 
import numpy
import sys

# Edit this as required (windows, mac).  Be able to write this.
PLY_FILE="/tmp/displaz_bbox.ply"

# write .ply file representing a box with specfied center and transform to f
def MakeBBox(v_center, transform, f):
  f.write("ply\n")
  f.write("format ascii 1.0\n")
  f.write("comment simple rectangular prism\n")
  f.write("element vertex 8\n")
  f.write("property float x\n")
  f.write("property float y\n")
  f.write("property float z\n")
  f.write("element face 2\n")
  f.write("property list uint int vertex_index\n")
  f.write("end_header\n")

  # apply transform to unit vectors and then add result to center
  unit_vectors = [ 
    [0, 1, -1, 0], [1, 0, -1, 0], [0, -1, -1, 0], [-1, 0, -1, 0], 
    [0, 1, 1, 0], [1, 0, 1, 0], [0, -1, 1, 0], [-1, 0, 1, 0]
  ] 
  xfm_unit_vectors = [transform.dot(unit) for unit in unit_vectors]
  vertices = [numpy.add(v_center, xuv) for xuv in xfm_unit_vectors]

  # write out resulting vertices
  map(lambda v: f.write("%s\n" % " ".join(str(elem) for elem in v)), vertices)
  f.write("4 0 1 2 3\n")
  f.write("4 4 5 6 7\n")

# write out the .ply file and have Displaz render it
def UpdateDisplaz(v, T):
    t = open(PLY_FILE, 'w')
    t.truncate()
    MakeBBox(v, T, t)
    t.close()
    p = subprocess.Popen(["displaz -script -rmtemp " + PLY_FILE], stdout=subprocess.PIPE, shell=True, bufsize=1)

def Usage():
  print "Usage:  DrawBBox <input point cloud file>\n"
  print "  Shift Click will set the 3D cursor and put a 1 x 1 x 1 BBox there.\n"
  print "   Do this first.\n"
  print "  Then you can press x, y and z to grow it.\n"
  print "  Remember that these keystrokes go into the Displayz application (needs focus)"
  print "\n\nTry it:  press space.\n"

def main():
  Usage()
  inputfile = ""
  if (len(sys.argv) > 1):
    inputfile = sys.argv[1]
  # launch the main Displaz server.  This the the GUI the user sees
  p = subprocess.Popen(["displaz -hook key:Space cursor -hook key:X key -hook key:Y key -hook key:Z key "+inputfile], stdout=subprocess.PIPE, shell=True, bufsize=1)

  # T is the augmented affine transformation matrix that does the scaling,
  #  rotation, shear and translation.  We start with Identity + 0 translation
  T = numpy.append(numpy.identity(3), numpy.array([0, 0, 0])[numpy.newaxis].T, 1)

  # main event loop
  center = []
  while True:
   # Read the output from the Displaz server and update the UI accordingly
   line = p.stdout.readline() # something like "key:Space cursor 0 0 0"
   #print("Read from stdout: {}\n".format(line))
   if not line:
     break;
   elif line.split()[0] == 'key:Space':
     # center is [x y z] for center of bbox
     center = [float(x) for x in line.split()[2:]]
     UpdateDisplaz(center, T)
   elif line.split()[0] == 'key:X':
     T[0][0] *= 1.5
     UpdateDisplaz(center, T)
   elif line.split()[0] == 'key:Y':
     T[1][1] *= 1.5
     UpdateDisplaz(center, T)
   elif line.split()[0] == 'key:Z':
     T[2][2] *= 1.5
     UpdateDisplaz(center, T)
   else:
     print "unexpected input from Displaz:", line.rstrip()

# entry point
main()
