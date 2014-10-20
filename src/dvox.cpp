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

#include <algorithm>
#include <fstream>
#include <queue>
#include <unordered_map>

#include "argparse.h"

#include "config.h"
#include "geomfield.h"
#include "hcloud.h"
#include "logger.h"
#include "util.h"
#include "pointdbwriter.h"
#include "pointdb.h"
#include "voxelizer.h"


std::vector<std::string> g_positionalArgs;
static int storePositionalArg (int argc, const char *argv[])
{
    for(int i = 0; i < argc; ++i)
        g_positionalArgs.push_back (argv[i]);
    return 0;
}


/// dvox: A batch voxelizer for unstructured point clouds
int main(int argc, char* argv[])
{
    Imath::V3d boundMin = Imath::V3d(0);
    double rootNodeWidth = 1000;
    float pointRadius = 0.2;
    int brickRes = 8;
    double leafNodeWidth = 2.5;

    double dbTileSize = 100;
    double dbCacheSize = 100;

    bool logProgress = false;
    int logLevel = Logger::Info;

    ArgParse::ArgParse ap;

    ap.options(
        "dvox - voxelize unstructured point clouds (version " DISPLAZ_VERSION_STRING ")\n"
        "\n"
        "Usage: dvox input1 [input2 ...] output\n"
        "\n"
        "input can be .las or .pointdb\n"
        "output can be .pointdb or .hcloud",
        "%*", storePositionalArg, "",

        "<SEPARATOR>", "\nVoxelization Options:",
        "-bound %F %F %F %F", &boundMin.x, &boundMin.y, &boundMin.z, &rootNodeWidth,
                                        "Bounding box for hcloud (min_x min_y min_z width)",
        "-pointradius %f", &pointRadius, "Assumed radius of points used during voxelization",
        "-brickresolution %d", &brickRes, "Resolution of octree bricks",
        "-leafnoderadius %F", &leafNodeWidth, "Desired width for octree leaf nodes",

        "<SEPARATOR>", "\nPoint Database options:",
        "-dbtilesize %F", &dbTileSize, "Tile size of temporary point database",
        "-dbcachesize %F", &dbCacheSize, "In-memory cache size for database in MB (default 100 MB)",

        "<SEPARATOR>", "\nInformational options:",
        "-loglevel %d",  &logLevel,    "Logger verbosity (default 3 = info, greater is more verbose)",
        "-progress",     &logProgress, "Log processing progress",

        NULL
    );

    StreamLogger logger(std::cerr);

    if (ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        ap.usage();
        logger.error("%s", ap.geterror());
        return EXIT_FAILURE;
    }
    if (argc == 1)
    {
        ap.usage();
        return EXIT_FAILURE;
    }

    logger.setLogLevel(Logger::LogLevel(logLevel));
    logger.setLogProgress(logProgress);

    try
    {
        if (g_positionalArgs.size() < 2)
        {
            logger.error("Expected at least two positional arguments");
            ap.usage();
            return EXIT_FAILURE;
        }
        std::string outputPath = g_positionalArgs.back();
        std::vector<std::string> inputPaths(g_positionalArgs.begin(),
                                            g_positionalArgs.end()-1);
        if (endswith(outputPath, ".pointdb"))
        {
            convertLasToPointDb(outputPath, inputPaths,
                                Imath::Box3d(), dbTileSize, logger);
        }
        else
        {
            if (!endswith(g_positionalArgs[0], ".pointdb") ||
                inputPaths.size() != 1)
            {
                logger.error("Need exactly one input .pointdb file");
                return EXIT_FAILURE;
            }
            if (!endswith(outputPath, ".hcloud"))
            {
                logger.error("Expected .hcloud file as output path");
                return EXIT_FAILURE;
            }
            SimplePointDb pointDb(inputPaths[0],
                                  (size_t)(dbCacheSize*1024*1024),
                                  logger);

            int leafDepth = floor(log(rootNodeWidth/leafNodeWidth)/log(2) + 0.5);
            logger.info("Leaf node width = %.3f", rootNodeWidth / (1 << leafDepth));
            std::ofstream outputFile(outputPath);
            voxelizePointCloud(outputFile, pointDb, pointRadius,
                               boundMin, rootNodeWidth,
                               leafDepth, brickRes, logger);
        }
    }
    catch (std::exception& e)
    {
        logger.error("Caught exception: %s", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

