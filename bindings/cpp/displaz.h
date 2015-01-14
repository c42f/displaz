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

#ifndef DISPLAZ_BINDING_H_INCLUDED
#define DISPLAZ_BINDING_H_INCLUDED

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#include <unistd.h>

/// C++11 bindings for displaz
///
/// The aim here is to make it simple to use displaz from an external C++
/// process to plot large numbers of points.  This is especially useful for
/// debugging geometric algorithms written in C++.
///
/// This interface is intentionally independent from the rest of the displaz
/// codebase to avoid any need for linking in a separate library - just include
/// this header and away you go.

namespace dpz {

namespace detail {

enum class PlyType
{
    uint8,
    uint16,
    uint32,
    int8,
    int16,
    int32,
    float32,
    float64,
};

template<typename T> struct PlyTypeMap { };
#define DISPLAZ_DEFINE_PLY_TYPE(cType, plyName)         \
template<> struct PlyTypeMap<cType> {                   \
    static const PlyType plyType = PlyType::plyName ;   \
    static const char* name() { return #plyName; }      \
};

DISPLAZ_DEFINE_PLY_TYPE(uint8_t,  uint8);
DISPLAZ_DEFINE_PLY_TYPE(uint16_t, uint16);
DISPLAZ_DEFINE_PLY_TYPE(uint32_t, uint32);
DISPLAZ_DEFINE_PLY_TYPE(int8_t,   int8);
DISPLAZ_DEFINE_PLY_TYPE(int16_t,  int16);
DISPLAZ_DEFINE_PLY_TYPE(int32_t,  int32);
DISPLAZ_DEFINE_PLY_TYPE(float,    float32);
DISPLAZ_DEFINE_PLY_TYPE(double,   float64);

#undef DISPLAZ_DEFINE_PLY_TYPE


struct PointAttribute
{
    PlyType plyType;
    std::string plyTypeName;
    std::string name;
    // TODO: vector semantics
    // TODO: unscaled integers
    int bytesPerBase;
    int count;
    int bytesPerPoint;

    template<typename T>
    static PointAttribute create(const std::string& name, int count)
    {
        PointAttribute attr;
        attr.plyType = PlyTypeMap<T>::plyType;
        attr.plyTypeName = PlyTypeMap<T>::name();
        attr.name = name;
        attr.bytesPerBase = sizeof(T);
        attr.count = count;
        attr.bytesPerPoint = attr.bytesPerBase*count;
        return attr;
    }

    void store(std::vector<char>& data, const double* inData)
    {
        size_t index = data.size();
        data.resize(data.size() + bytesPerPoint);
        switch(plyType)
        {
#           define DISPLAZ_DOCOPY(ctype)                         \
                {                                                \
                    ctype* buf = (ctype*) &data[index];          \
                    for (int i = 0; i < count; ++i)              \
                        buf[i] = ctype(inData[i]);               \
                }
            case PlyType::uint8:   DISPLAZ_DOCOPY(uint8_t);  return;
            case PlyType::uint16:  DISPLAZ_DOCOPY(uint16_t); return;
            case PlyType::uint32:  DISPLAZ_DOCOPY(uint32_t); return;
            case PlyType::int8:    DISPLAZ_DOCOPY(int8_t);   return;
            case PlyType::int16:   DISPLAZ_DOCOPY(int16_t);  return;
            case PlyType::int32:   DISPLAZ_DOCOPY(int32_t);  return;
            case PlyType::float32: DISPLAZ_DOCOPY(float);    return;
            case PlyType::float64: DISPLAZ_DOCOPY(double);   return;
#           undef DISPLAZ_DOCOPY
        }
    }
};

}


/// List of points to be plotted
class PointList
{
    public:
        /// Add an attribute to each point
        ///
        /// This may only be called before any points are added using append()
        /// `name` is the name of the point attribute; `count` is the number of
        /// elements (for example, this should be 3 for a 3D vector)
        template<typename T>
        PointList& addAttribute(const std::string& name, int count = 1)
        {
            if (count > 4)
                throw std::runtime_error("Displaz can't display vector attributes of length > 4");
            if (m_data.size() > 0 && m_data[0].size() > 0)
                throw std::runtime_error("Cannot add attribute to nonempty point list");
            m_attributes.push_back(detail::PointAttribute::create<T>(name, count));
            m_data.push_back(std::vector<char>());
            return *this;
        }

        /// Append a new point to the list
        ///
        /// The values of all attributes added using `addAttribute()` must be
        /// provided in the argument list `args`.
#       if _MSC_FULL_VER >= 180020827
        template<typename... Args>
        void append(const Args&... args)
        {
            double values[] = {double(args)...};
            size_t numVals = sizeof...(args);
            append(values, numVals);
        }
#       endif

        /// Append a new point to the list
        ///
        /// See append() above for a more convenient way to do this
        void append(const double* values, size_t numVals)
        {
            for (size_t i = 0, j = 0; i < m_attributes.size(); ++i)
            {
                detail::PointAttribute& attr = m_attributes[i];
                attr.store(m_data[i], &values[j]);
                if (j + attr.count > numVals)
                    throw std::runtime_error("Not enough values when adding point to point list");
                j += attr.count;
            }
        }

        /// Write point list to a file in native displaz format
        void writeToFile(FILE* ply) const
        {
            fprintf(ply,
                "ply\n"
                "format binary_%s_endian 1.0\n"
                "comment Displaz native\n",
                isLittleEndian() ? "little" : "big"
            );
            const char* propNames[] = {"x", "y", "z", "w"};
            for (size_t i = 0; i < m_attributes.size(); ++i)
            {
                const detail::PointAttribute& attr = m_attributes[i];
                fprintf(ply, "element vertex_%s "
#                       ifdef _MSC_VER
                        "%Iu"
#                       else
                        "%zu"
#                       endif
                        "\n",
                        attr.name.c_str(),
                        m_data[i].size()/attr.bytesPerPoint
                );
                for (int j = 0; j < attr.count; ++j)
                {
                    fprintf(ply, "property %s %s\n",
                            attr.plyTypeName.c_str(),
                            propNames[j]);
                }
            }
            fprintf(ply, "end_header\n");
            for (size_t i = 0; i < m_data.size(); ++i)
            {
                if (fwrite(m_data[i].data(), 1, m_data[i].size(), ply) != m_data[i].size())
                    throw std::runtime_error("Error writing point attribute");
            }
        }

    private:
        static inline int isLittleEndian()
        {
            union { uint16_t i; char c[2]; } u;
            u.i = 0x0001;
            return u.c[0];
        }

        std::vector<detail::PointAttribute> m_attributes;
        std::vector<std::vector<char>> m_data;
};


/// Class representing a remote displaz process
class Displaz
{
    private:
        /// Smart pointer to C file.  We're using C file IO, since we then have
        /// a more convenient way to get a temporary file
        typedef std::unique_ptr<FILE, int (&)(FILE*)> FilePtr;

    public:
        /// Set up interface to talk to displaz instance named `windowName`
        Displaz(const std::string& windowName = "")
            : m_windowName(windowName),
            m_hold(true),
            m_debug(false)
        {
            // FIXME: Bug: can't yet start displaz window here, due to race
            // condition on socket...
            //sendMessage("");
        }

        /// Set plotting hold state so that plots will be overwritten or
        /// concatenated into the window depending on the value of `holdPlots`
        void hold(bool holdPlots = true)
        {
            m_hold = holdPlots;
        }

        /// Set shader to use
        void setShader(const std::string& shaderName)
        {
            m_shaderName = shaderName;
        }

        /// Plot a list of points into the displaz window
        void plot(PointList& points)
        {
            std::string fileName;
            FilePtr ply = openTempPly(fileName);
            points.writeToFile(ply.get());
            std::string opts = fileName;
            if (m_hold)
                opts += " -add";
            if (!m_debug)
                opts += " -rmtemp";
            if (!m_shaderName.empty())
                opts += " -shader \"" + m_shaderName + "\"";
            sendMessage(opts);
        }

        /// Enable debug logging and disable deletion of temporary files
        void setDebug(bool debugMode)
        {
            m_debug = debugMode;
        }

    private:
        /// Launch a displaz process in the background with given options
        void sendMessage(const std::string& options) const
        {
            std::string cmd;
            std::string opts = options;
            if (!m_windowName.empty())
                opts += " -server \"" + m_windowName + "\"";
#           ifdef _WIN32
            // needs testing
            cmd = "start /b displaz " + opts;
#           else
            cmd = "displaz " + opts + " &";
#           endif
            if (m_debug)
                std::cout << cmd << "\n";
            if (system(cmd.c_str()) != 0)
            {
                std::cerr << "Error launching displaz command:\n"
                          << cmd << "\n";
            }
        }

        /// Open a temporary FILE with name ending in .ply
        ///
        /// The full file name is returned in `fileName`.
        static FilePtr openTempPly(std::string& fileName)
        {
#           ifdef _WIN32
#           pragma message("TODO: Implement proper temporary file name support")
            fileName = "_displaz_temp.ply";
            FilePtr ply(fopen(fileName.c_str(), "wb"), fclose);
#           else
            fileName = "/tmp/displaz_cpp_XXXXXX.ply";
            int plyfd = mkstemps(&fileName[0], 4);
            FilePtr ply(fdopen(plyfd, "wb"), fclose);
#           endif
            return std::move(ply);
        }

        std::string m_windowName;
        std::string m_shaderName;
        bool m_hold;
        bool m_debug;
};


} // namespace dpz


#endif // DISPLAZ_BINDING_H_INCLUDED
