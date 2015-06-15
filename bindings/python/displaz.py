import subprocess
import numpy as np
import os
import tempfile

# Very rudimentary matplotlib-style plotting interface.  Currently sufficient
# for simple display of points but not much more.


def _interpret_spec(specstr):
    colors = {
        'r': [1, 0, 0],
        'g': [0, 1, 0],
        'b': [0, 0, 0.8],
        'c': [0, 1, 1],
        'm': [1, 0, 1],
        'y': [1, 1, 0],
        'k': [0, 0, 0],
        'w': [1, 1, 1],
    }
    markershapes = {
        '.': 0,
        's': 1,
        'o': 2,
    }
    spec = {}
    for c in specstr:
        if colors.has_key(c):
            spec['color'] = np.array(colors[c], dtype=np.float32)
        elif markershapes.has_key(c):
            spec['markershape'] = np.array(markershapes[c], dtype=np.int32)
        else:
            raise Exception('Plot spec %s not recognized' % (c,))
    return spec


def _call_displaz(*args):
    subprocess.Popen(['displaz'] + list(args))


def _write_ply(plyfile, position, color):
    N = position.shape[0]
    plyfile.write(
r'''ply
format binary_little_endian 1.0
comment Displaz native
element vertex_position %d
property float64 x
property float64 y
property float64 z
element vertex_color %d
property float64 r
property float64 g
property float64 b
end_header
''' % (N, N))
    position.tofile(plyfile)
    color.tofile(plyfile)


__hold_plot = False

def hold(state=None):
    global __hold_plot
    if state is not None:
        __hold_plot = state
    else:
        __hold_plot = not __hold_plot


def plot(position, plotspec='b.', color=None):
    spec = _interpret_spec(plotspec)
    if color is None:
        color = spec['color']
    if len(color.shape) == 1:
        color = np.tile(color, (position.shape[0],1))
    if position.shape[1] != 3 or color.shape[1] != 3:
        raise Exception('Position and color must have three columns each')
    if position.dtype != np.float64:
        position = np.float64(position)
    if color.dtype != np.float64:
        color = np.float64(color)
    fd, filename = tempfile.mkstemp(suffix='.ply', prefix='displaz_py_')
    plyfile = os.fdopen(fd, 'w')
    _write_ply(plyfile, position, color)
    plyfile.close()
    args = ['-shader', 'generic_points.glsl', '-rmtemp', filename]
    if __hold_plot:
        args = ['-add'] + args
    _call_displaz(*args)


def multi_plot(position_list):
    """
    Plots different points clouds with random colors
    
    Args:
        position_list (list(numpy.array)): list of point clouds
    """
    if not isinstance(position_list, list):
        raise TypeError("Input should be a list of points clouds")
    file_names = []    
    for i, position in enumerate(position_list):
        if position.shape[1] != 3:
            raise Exception('Position must have three columns')
        if position.dtype != np.float64:
            position = np.float64(position)
        color = np.tile(np.random.random_sample(3), (position.shape[0],1) )
        
        fd, filename = tempfile.mkstemp(suffix='.ply', prefix='displaz_py_')
        file_names.append(filename)
        plyfile = os.fdopen(fd, 'w')
        _write_ply(plyfile, position, color)
        plyfile.close()
    
    args = ['-shader', 'las_points.glsl', '-rmtemp'] + file_names
    if __hold_plot:
        args = ['-add'] + args
    _call_displaz(*args)


def clf():
    _call_displaz('-clear')

