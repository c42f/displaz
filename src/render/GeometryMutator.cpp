// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "GeometryMutator.h"
#include "ply_io.h"
#include "QtLogger.h"

GeometryMutator::GeometryMutator()
    : m_npoints(0),
    m_indexFieldIdx(-1),
    m_index(0)
{ }


GeometryMutator::~GeometryMutator()
{ }



bool GeometryMutator::loadFile(const QString& fileName)
{
    // Check it is ply
    if (!fileName.endsWith(".ply"))
    {
        g_logger.error("Expected ply for file %s", fileName);
        return false;
    }

    try
    {
        std::unique_ptr<t_ply_, int(*)(p_ply)> ply(
                ply_open(fileName.toUtf8().constData(), logRplyError, 0, NULL), ply_close);
        if (!ply || !ply_read_header(ply.get()))
            return false;
        // Parse out header data
        p_ply_element vertexElement = findVertexElement(ply.get(), m_npoints);
        if (vertexElement)
        {
            g_logger.error("Expected displaz formatted ply for file %s", fileName);
            return false;
        }
        else
        {
            if (!loadDisplazNativePly(fileName, ply.get(), m_fields, m_offset, m_npoints))
                return false;
        }
    }
    catch(...)
    {
        g_logger.error("Unkown load error for file %s", fileName);
        return false;
    }

    // Search for index field
    m_indexFieldIdx = -1;
    for (size_t i = 0; i < m_fields.size(); ++i)
    {
        if (m_fields[i].name == "index")
        {
            if (!(m_fields[i].spec == TypeSpec::uint32()))
            {
                g_logger.error("The \"index\" field found in file %s is not of type uint32", fileName);
                return false;
            }
            m_indexFieldIdx = (int)i;
            break;
        }
    }
    if (m_indexFieldIdx == -1)
    {
        g_logger.error("No \"index\" field found in file %s", fileName);
        return false;
    }
    m_index = m_fields[m_indexFieldIdx].as<uint32_t>();

    g_logger.info("Loaded %d point mutations from file %s",
                  m_npoints, fileName);

    return true;
}
