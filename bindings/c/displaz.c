// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


int is_little_endian()
{
    union
    {
        uint16_t i;
        char c[2];
    } u;
    u.i = 0x0001;
    return u.c[0];
}


/// Like displaz_write_ply, but takes a FILE stream
int displaz_fwrite_ply(FILE* ply, size_t npoints,
                       double* position, float* color, float* normal)
{
    fprintf(ply,
        "ply\n"
        "format binary_%s_endian 1.0\n"
        "comment Displaz native\n"
        "element vertex_position %zu\n"
        "property double x\n"
        "property double y\n"
        "property double z\n",
        is_little_endian() ? "little" : "big",
        npoints
    );
    if (color)
    {
        fprintf(ply,
            "element vertex_color %zu\n"
            "property float r\n"
            "property float g\n"
            "property float b\n",
            npoints
        );
    }
    if (normal)
    {
        fprintf(ply,
            "element vertex_normal %zu\n"
            "property float x\n"
            "property float y\n"
            "property float z\n",
            npoints
        );
    }
    fprintf(ply, "end_header\n");
    if (fwrite(position, sizeof(double)*3, npoints, ply) != npoints)
        return 0;
    if (color)
    {
        if (fwrite(color, sizeof(float)*3, npoints, ply) != npoints)
            return 0;
    }
    if (normal)
    {
        if (fwrite(normal, sizeof(float)*3, npoints, ply) != npoints)
            return 0;
    }
    return 1;
}


/// Very simplistic C interface to write displaz native ply format
///
/// npoints - Number of points in each of position, color and normal
/// position - Array of length 3*npoints containing position data.  Storage
///            order is [x1 y1 z1 x2 y2 z2 ...]
/// color - Array of length 3*npoints containing color data.  May be null
/// normal - Array of length 3*npoints containing normal data.  May be null
int displaz_write_ply(const char* fileName, size_t npoints,
                      double* position, float* color, float* normal)
{
    FILE* ply = fopen(fileName, "w");
    if (!ply)
        return 0;
    int ret = displaz_fwrite_ply(ply, npoints, position, color, normal);
    fclose(ply);
    return ret;
}


/// Launch a displaz process in the background to open a file
///
/// options are any extra command line options in a string.  If null, options
/// is set to "-add".
int launch_displaz(const char* fileName, const char* options)
{
    if (!options)
        options = "-add";
    char cmd[1024];
    // TODO: Make displaz itself do the fork & launch in the background
    // internally for consistency between first time startup and sending data
    // via the socket?
#ifdef _WIN32
    // needs testing
    _snprintf(cmd, 1024, "start /b displaz %s %s", options, fileName);
#else
    snprintf(cmd, 1024, "displaz %s %s &", options, fileName);
#endif
    cmd[1023] = '\0';
    //printf("%s\n", cmd);
    return system(cmd);
}


/// Start displaz to display the given set of points
///
/// npoints - Number of points in each of position, color and normal
/// position - Array of length 3*npoints containing position data.  Storage
///            order is [x1 y1 z1 x2 y2 z2 ...]
/// color - Array of length 3*npoints containing color data.  May be null
/// normal - Array of length 3*npoints containing normal data.  May be null
int displaz_points(size_t npoints, double* position, float* color, float* normal)
{
#ifdef _WIN32
#warning "TODO: Implement proper temporary file name support"
    char fileName[] = "_displaz_temp.ply";
    FILE* ply = fopen(fileName, "wb");
#else
    char fileName[] = "/tmp/displaz_c_XXXXXX.ply";
    int plyfd = mkstemps(fileName, 4);
    FILE* ply = fdopen(plyfd, "wb");
#endif
    if (!displaz_fwrite_ply(ply, npoints, position, color, normal))
        return 1;
    fclose(ply);
    return launch_displaz(fileName, "-add -rmtemp");
}


