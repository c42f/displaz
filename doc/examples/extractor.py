#!/usr/bin/env python

# Copyright 2017, Michael S Rosen / Azimetry and the other displaz contributors.
# Use of this code is governed by the BSD-style license found in LICENSE.txt
import argparse
import json
import math
import os
import subprocess
import sys
import tempfile
import textwrap

import numpy

INSTRUCTIONS = """
    This script launches Displaz and loads a point cloud allowing you to interactively select a 
    3D extent and export the corresponding points.  It assumes you have pdal and displaz in your $PATH.

    Keystrokes need to go into the Displaz window, not this script

    Space:  Sets the initial bound box:  2x2x2 centered at the 3D cursor position 
    (set via Shift Click in Displaz).  Do this first.  Then interactively, use the
    following key commands:

      X, Y or Z:  Scales (bigger) the box along the corresponding axis.
      CNTL X, Y, or Z:  Translates (bigger) the box
      ALT X, Y, or Z:  Rotates (clockwise) the box
      SHIFT + (above):  The Shift modifier inverts the above transformations
                      (smaller, ccw)
      E:  Extract the points in the current box to specified output file.

      Q:  Exit the script.  You'll need to shut down Displaz separately.
    """

class Displaz:
    SCRIPT_CMDLINE = ['displaz', '-script', '-rmtemp', ]

    VIEWER_CMDLINE = [
        'displaz',
        '-hook', 'key:Space', 'cursor',
        '-hook', 'key:X', 'key',
        '-hook', 'key:Y', 'key',
        '-hook', 'key:Z', 'key',
        '-hook', 'key:Shift+X', 'key',
        '-hook', 'key:Shift+Y', 'key',
        '-hook', 'key:Shift+Z', 'key',
        '-hook', 'key:Ctrl+X', 'key',
        '-hook', 'key:Ctrl+Y', 'key',
        '-hook', 'key:Ctrl+Z', 'key',
        '-hook', 'key:Shift+Ctrl+X', 'key',
        '-hook', 'key:Shift+Ctrl+Y', 'key',
        '-hook', 'key:Shift+Ctrl+Z', 'key',
        '-hook', 'key:Alt+X', 'key',
        '-hook', 'key:Alt+Y', 'key',
        '-hook', 'key:Alt+Z', 'key',
        '-hook', 'key:Shift+Alt+X', 'key',
        '-hook', 'key:Shift+Alt+Y', 'key',
        '-hook', 'key:Shift+Alt+Z', 'key',
        '-hook', 'key:E', 'key',
        '-hook', 'key:Q', 'key',
    ]

    RECTANGULAR_PRISM_HEADER = """
    ply
    format ascii 1.0
    comment simple rectangular prism
    element vertex 8
    property float x
    property float y
    property float z
    property float r
    property float g
    property float b
    element face 2
    property list uint int vertex_index
    element edge 4
    property list uchar int vertex_index
    end_header
    """
    RECTANGULAR_PRISM_HEADER = textwrap.dedent(RECTANGULAR_PRISM_HEADER)
    RECTANGULAR_PRISM_HEADER = RECTANGULAR_PRISM_HEADER.strip()

    DELTA_THETA = math.pi/18.0
    DELTA_XYZ = 5.0
    FACTOR_XYZ = 1.5

    def __init__(self, *args, **kwargs):
        self.input = kwargs['input']
        self.output = kwargs['output']
        self.ply_file = tempfile.NamedTemporaryFile(suffix='.ply', delete=False).name
        cmdline = self.VIEWER_CMDLINE + [self.input]
        self.viewer = subprocess.Popen(
            cmdline, stdout=subprocess.PIPE, shell=False, bufsize=1
        )
        print('{}'.format(textwrap.dedent(INSTRUCTIONS)))
        self.pdal = Pdal(output=self.output)
        self.event_loop()

    def make_bbox(self, center, transform):
        # add result to center of prism
        unit_vectors = [
          [1, 1, -1, 1], [-1, 1, -1, 1], [-1, -1, -1, 1], [1, -1, -1, 1],
          [1, 1, 1, 1],  [-1, 1, 1, 1],  [-1, -1, 1, 1],  [1, -1, 1, 1]
        ]

        with open(self.ply_file, 'w') as fh:
            fh.write('{}\n'.format(self.RECTANGULAR_PRISM_HEADER))

            center = numpy.array(center + [0])
            count = 0
	    x_range = [float('inf'), float('-inf')]
	    y_range = [float('inf'), float('-inf')]
	    z_range = [float('inf'), float('-inf')] 


            # import pdb; pdb.set_trace()
            for i, unit_vector in enumerate(unit_vectors):
                vertex = center + transform.dot(unit_vector)
                colors = [1, 0, 0] if i < 4 else [0, 1, 0]
                vertex = list(vertex[:3]) + colors
		    
		x_range[0] = min(x_range[0], vertex[0])
		x_range[1] = max(x_range[1], vertex[0])
		y_range[0] = min(y_range[0], vertex[1])
		y_range[1] = max(y_range[1], vertex[1])
		z_range[0] = min(z_range[0], vertex[2])
		z_range[1] = max(z_range[1], vertex[2])

                fh.write('{} {} {} {} {} {}\n'.format(*vertex))

            self.bounds = (x_range, y_range, z_range)

            # define two opposing faces
            fh.write('4 0 1 2 3 \n')
            fh.write('4 4 5 6 7 \n')

            # define four edges connecting the corresponding vertices
            # from the two faces defined above
            fh.write('2 0 4 \n')
            fh.write('2 1 5 \n')
            fh.write('2 2 6 \n')
            fh.write('2 3 7 \n')

        return

    def update_viewer(self, center, T):
        self.make_bbox(center, T)
        cmdline = self.SCRIPT_CMDLINE + [self.ply_file]
        script = subprocess.Popen(
            cmdline, stdout=subprocess.PIPE, shell=False, bufsize=1
        )

        return

    def apply_rotation(self, theta, axis, T):
        c = math.cos(theta)
        s = math.sin(theta)
        R = numpy.identity(4)

        if axis == 'X':
            R[1][1] = c
            R[1][2] = -s
            R[2][1] = s
            R[2][2] = c
        elif axis == 'Y':
            R[0][0] = c
            R[0][2] = s
            R[2][0] = -s
            R[2][2] = c
        elif axis == 'Z':
            R[0][0] = c
            R[0][1] = -s
            R[1][0] = s
            R[1][1] = c
        else:
            raise ValueError('invalid axis: {}'.format(axis))

        # Note: rotation must be applied last
        T = R.dot(T)

        return T

    def event_loop(self):
        """
        Update viewer based on output from the Displaz server.
        Each line resembles 'key:Space cursor 0 0 0'
        """

        # T is the augmented affine transformation matrix
        # It scales, rotates and translates
        T = numpy.identity(4)

        exit_code = 0
        theta = 0
        center = [0, 0, 0]
        while True:
            line = self.viewer.stdout.readline()
            line = line.rstrip()

            if not line:
                break

            key_press = line.split()[0]
            if key_press == 'key:Space':
                # center is [x y z] for center of bbox
                center = [float(x) for x in line.split()[2:]]
            elif key_press == 'key:X':
                T[0][0] *= self.FACTOR_XYZ
            elif key_press == 'key:Y':
                T[1][1] *= self.FACTOR_XYZ
            elif key_press == 'key:Z':
                T[2][2] *= self.FACTOR_XYZ
            elif key_press == 'key:Shift+X':
                T[0][0] /= self.FACTOR_XYZ
            elif key_press == 'key:Shift+Y':
                T[1][1] /= self.FACTOR_XYZ
            elif key_press == 'key:Shift+Z':
                T[2][2] /= self.FACTOR_XYZ
            elif key_press == 'key:Ctrl+X':
                T[0][3] += self.DELTA_XYZ
            elif key_press == 'key:Ctrl+Y':
                T[1][3] += self.DELTA_XYZ
            elif key_press == 'key:Ctrl+Z':
                T[2][3] += self.DELTA_XYZ
            elif key_press == 'key:Shift+Ctrl+X':
                T[0][3] -= self.DELTA_XYZ
            elif key_press == 'key:Shift+Ctrl+Y':
                T[1][3] -= self.DELTA_XYZ
            elif key_press == 'key:Shift+Ctrl+Z':
                T[2][3] -= self.DELTA_XYZ
            elif key_press == 'key:Alt+X':
                theta = self.DELTA_THETA
                T = self.apply_rotation(theta, 'X', T)
            elif key_press == 'key:Alt+Y':
                theta = self.DELTA_THETA
                T = self.apply_rotation(theta, 'Y', T)
            elif key_press == 'key:Alt+Z':
                theta = self.DELTA_THETA
                T = self.apply_rotation(theta, 'Z', T)
            elif key_press == 'key:Shift+Alt+X':
                theta = -self.DELTA_THETA
                T = self.apply_rotation(theta, 'X', T)
            elif key_press == 'key:Shift+Alt+Y':
                theta = -self.DELTA_THETA
                T = self.apply_rotation(theta, 'Y', T)
            elif key_press == 'key:Shift+Alt+Z':
                theta = -self.DELTA_THETA
                T = self.apply_rotation(theta, 'Z', T)
            elif key_press == 'key:E':
                exit_code = self.pdal.crop(self.input, self.bounds)
            elif key_press == 'key:Q':
                # Displaz seems to not respond to SIGTERM.  Hmm.
                self.viewer.kill()
                quit()
            else:
                print('Unexpected input from displaz: {}'.format(line))
                continue

            self.update_viewer(center, T)

class Pdal:
    def __init__(self, *args, **kwargs):
        self.output = kwargs['output']
    def crop(self, input, bounds):
        self.input = input
        self.bounds = bounds 
        corner = [comp[0] for comp in bounds]
        corner = '_'.join(str(v) for v in corner)
        if not self.output:
            self.output = os.path.basename(input) 
        self.output = os.path.splitext(self.output)[0]
        self.output = '{}_{}.laz'.format(self.output, corner)
        stage_crop = {
            'type': 'filters.crop',
            'bounds': str(self.bounds),
        }
        pipeline = {
            'pipeline':[self.input, stage_crop, self.output, ]
        }
        pipeline = json.dumps(pipeline)
        with open('pspec.json', 'w') as fh:
            fh.write(pipeline)
        args = ['pdal', 'pipeline', 'pspec.json']
        proc = subprocess.Popen(args, shell=False)
        print ('Extracting bounds {} from {} ...').format(bounds, input)
        exit_code = proc.wait()
        if exit_code == 0:
           print('  ... Successful: {}.'.format(self.output))
        else:
           print('  ... Failed!  PDAL exit code {}.'.format(self.exit_code))

        return exit_code

class Extractor:
    def __init__(self, *args, **kwargs):
        self.input = kwargs['input']
        self.output = kwargs['output']


        self.displaz = Displaz(input=self.input, output=self.output)



def main():
    description = 'PointCloud Extraction Utility'
    parser = argparse.ArgumentParser(description=description)

    parser.add_argument('-i', '--input', dest='input')
    parser.add_argument('-o', '--output', dest='output', help = 'basename, will be suffixed with coords and .laz')

    args = parser.parse_args()

    input = args.input
    output = args.output

    extractor = Extractor(input=input, output=output)

    return


if __name__ == '__main__':
    retval = main()
    sys.exit()
