// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "ply_io.h"

#include <cstdint>

#include "QtLogger.h"

//------------------------------------------------------------------------------
// Utilities for interfacing with rply

/// Callback handler for reading ply data into a point field using rply
class PlyFieldLoader
{
    public:
        PlyFieldLoader(GeomField& field)
            : m_field(&field),
            m_pointIndex(0),
            m_componentReadCount(0),
            m_isPositionField(field.name == "position")
        { }

        /// rply callback to accept a single element of a point field
        static int rplyCallback(p_ply_argument argument)
        {
            void* pinfo = 0;
            long idata = 0;
            ply_get_argument_user_data(argument, &pinfo, &idata);
            double value = ply_get_argument_value(argument);
            return pinfo ? ((PlyFieldLoader*)pinfo)->writeValue(idata, value) : 0;
        }

        /// Accept a single element of a point field from the ply file
        int writeValue(int componentIndex, double value)
        {
            if (m_isPositionField)
            {
                // Remove fixed offset using first value read.
                if (m_pointIndex == 0)
                    m_offset[componentIndex] = value;
                value -= m_offset[componentIndex];
            }
            // Set data value depending on type
            size_t idx = m_pointIndex*m_field->spec.count + componentIndex;
            switch(m_field->spec.type)
            {
                case TypeSpec::Int:
                    switch(m_field->spec.elsize)
                    {
                        case 1: m_field->as<int8_t>()[idx]  = (int8_t)value;  break;
                        case 2: m_field->as<int16_t>()[idx] = (int16_t)value; break;
                        case 4: m_field->as<int32_t>()[idx] = (int32_t)value; break;
                    }
                    break;
                case TypeSpec::Uint:
                    switch(m_field->spec.elsize)
                    {
                        case 1: m_field->as<uint8_t>()[idx]  = (uint8_t)value;  break;
                        case 2: m_field->as<uint16_t>()[idx] = (uint16_t)value; break;
                        case 4: m_field->as<uint32_t>()[idx] = (uint32_t)value; break;
                    }
                    break;
                case TypeSpec::Float:
                    switch(m_field->spec.elsize)
                    {
                        case 4: m_field->as<float>()[idx] = (float)value;   break;
                        case 8: m_field->as<double>()[idx] = (double)value; break;
                    }
                    break;
                default:
                    assert(0 && "Unknown type encountered");
            }
            // Keep track of which point we're on
            ++m_componentReadCount;
            if (m_componentReadCount == m_field->spec.count)
            {
                ++m_pointIndex;
                m_componentReadCount = 0;
            }
            return 1;
        }

        V3d offset() const { return V3d(m_offset[0], m_offset[1], m_offset[2]); }

    private:
        GeomField* m_field;
        size_t m_pointIndex;
        int m_componentReadCount;
        bool m_isPositionField;
        double m_offset[3];
};


/// Get TypeSpec type info from associated rply type
void plyTypeToPointFieldType(e_ply_type& plyType, TypeSpec::Type& type, int& elsize)
{
    switch(plyType)
    {
        case PLY_INT8:    case PLY_CHAR:   type = TypeSpec::Int;   elsize = 1; break;
        case PLY_INT16:   case PLY_SHORT:  type = TypeSpec::Int;   elsize = 2; break;
        case PLY_INT32:   case PLY_INT:    type = TypeSpec::Int;   elsize = 4; break;
        case PLY_UINT8:   case PLY_UCHAR:  type = TypeSpec::Uint;  elsize = 1; break;
        case PLY_UINT16:  case PLY_USHORT: type = TypeSpec::Uint;  elsize = 2; break;
        case PLY_UIN32:   case PLY_UINT:   type = TypeSpec::Uint;  elsize = 4; break;
        case PLY_FLOAT32: case PLY_FLOAT:  type = TypeSpec::Float; elsize = 4; break;
        case PLY_FLOAT64: case PLY_DOUBLE: type = TypeSpec::Float; elsize = 8; break;
        default: assert(0 && "Unknown ply type");
    }
}


//------------------------------------------------------------------------------
// Utilities for loading point fields from the "vertex" element

/// Find "vertex" element in ply file
p_ply_element findVertexElement(p_ply ply, size_t& npoints)
{
    for (p_ply_element elem = ply_get_next_element(ply, NULL);
         elem != NULL; elem = ply_get_next_element(ply, elem))
    {
        const char* name = 0;
        long ninstances = 0;
        if (!ply_get_element_info(elem, &name, &ninstances))
            continue;
        //tfm::printf("element %s, ninstances = %d\n", name, ninstances);
        if (strcmp(name, "vertex") == 0)
        {
            npoints = ninstances;
            return elem;
        }
    }
    return NULL;
}


/// Mapping from ply field names to internal (name,index)
struct PlyPointField
{
    std::string displazName;
    int componentIndex;
    TypeSpec::Semantics semantics;
    std::string plyName;
    e_ply_type plyType;
    bool fixedPoint;
};


/// Parse ply point properties, and recognize standard names
static std::vector<PlyPointField> parsePlyPointFields(p_ply_element vertexElement)
{
    // List of some fields which might be found in a .ply file and mappings to
    // displaz field groups.  Note that there's no standard!
    PlyPointField standardFields[] = {
        {"position",  0,  TypeSpec::Vector,  "x",      PLY_FLOAT, false},
        {"position",  1,  TypeSpec::Vector,  "y",      PLY_FLOAT, false},
        {"position",  2,  TypeSpec::Vector,  "z",      PLY_FLOAT, false},
        {"color",     0,  TypeSpec::Color ,  "red",    PLY_UINT8, true},
        {"color",     1,  TypeSpec::Color ,  "green",  PLY_UINT8, true},
        {"color",     2,  TypeSpec::Color ,  "blue",   PLY_UINT8, true},
        {"color",     0,  TypeSpec::Color ,  "r",      PLY_UINT8, true},
        {"color",     1,  TypeSpec::Color ,  "g",      PLY_UINT8, true},
        {"color",     2,  TypeSpec::Color ,  "b",      PLY_UINT8, true},
        {"normal",    0,  TypeSpec::Vector,  "nx",     PLY_FLOAT, false},
        {"normal",    1,  TypeSpec::Vector,  "ny",     PLY_FLOAT, false},
        {"normal",    2,  TypeSpec::Vector,  "nz",     PLY_FLOAT, false},
    };
    QRegExp vec3ComponentPattern("(.*)_?([xyz])");
    QRegExp arrayComponentPattern("(.*)\\[([0-9]+)\\]");
    size_t numStandardFields = sizeof(standardFields)/sizeof(standardFields[0]);
    std::vector<PlyPointField> fieldInfo;
    for (p_ply_property prop = ply_get_next_property(vertexElement, NULL);
         prop != NULL; prop = ply_get_next_property(vertexElement, prop))
    {
        const char* propName = 0;
        e_ply_type propType;
        if (!ply_get_property_info(prop, &propName, &propType, NULL, NULL))
            continue;
        if (propType == PLY_LIST)
        {
            g_logger.warning("Ignoring list property %s in ply file", propName);
            continue;
        }
        bool isStandardField = false;
        for (size_t i = 0; i < numStandardFields; ++i)
        {
            if (iequals(standardFields[i].plyName, propName))
            {
                fieldInfo.push_back(standardFields[i]);
                fieldInfo.back().plyType = propType;
                fieldInfo.back().plyName = propName; // ensure correct string case
                isStandardField = true;
                break;
            }
        }
        if (!isStandardField)
        {
            // Try to guess whether this is one of several components - if so,
            // it will be turned into an array type (such as a vector)
            std::string displazName = propName;
            int index = 0;
            TypeSpec::Semantics semantics = TypeSpec::Array;
            if (vec3ComponentPattern.exactMatch(propName))
            {
                displazName = vec3ComponentPattern.cap(1).toStdString();
                index = vec3ComponentPattern.cap(2)[0].toLatin1() - 'x';
                semantics = TypeSpec::Vector;
            }
            else if(arrayComponentPattern.exactMatch(propName))
            {
                displazName = arrayComponentPattern.cap(1).toStdString();
                index = arrayComponentPattern.cap(2).toInt();
            }
            PlyPointField field = {displazName, index, semantics, propName, propType, false};
            fieldInfo.push_back(field);
        }
    }
    return fieldInfo;
}


/// Order PlyPointField by displaz field name and component index
bool displazFieldComparison(const PlyPointField& a, const PlyPointField& b)
{
    if (a.displazName != b.displazName)
        return a.displazName < b.displazName;
    if (a.componentIndex != b.componentIndex)
        return a.componentIndex < b.componentIndex;
    return a.semantics < b.semantics;
}


bool loadPlyVertexProperties(QString fileName, p_ply ply, p_ply_element vertexElement,
                             std::vector<GeomField>& fields, V3d& offset,
                             size_t npoints)
{
    // Create displaz GeomField for each property of the "vertex" element
    std::vector<PlyPointField> fieldInfo = parsePlyPointFields(vertexElement);
    std::sort(fieldInfo.begin(), fieldInfo.end(), &displazFieldComparison);
    std::vector<PlyFieldLoader> fieldLoaders;
    // Hack: use reserve to avoid iterator invalidation in push_back()
    fields.reserve(fieldInfo.size());
    fieldLoaders.reserve(fieldInfo.size());
    // Always add position field
    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
    fieldLoaders.push_back(PlyFieldLoader(fields[0]));
    bool hasPosition = false;
    // Add all other fields, and connect fields to rply callbacks
    for (size_t i = 0; i < fieldInfo.size(); )
    {
        const std::string& fieldName = fieldInfo[i].displazName;
        TypeSpec::Semantics semantics = fieldInfo[i].semantics;
        TypeSpec::Type baseType = TypeSpec::Unknown;
        bool fixedPoint = fieldInfo[i].fixedPoint;
        int elsize = 0;
        plyTypeToPointFieldType(fieldInfo[i].plyType, baseType, elsize);
        size_t eltBegin = i;
        int maxComponentIndex = 0;
        while (i < fieldInfo.size() && fieldInfo[i].displazName == fieldName &&
               fieldInfo[i].semantics == semantics)
        {
            maxComponentIndex = std::max(maxComponentIndex,
                                         fieldInfo[i].componentIndex);
            ++i;
        }
        size_t eltEnd = i;
        PlyFieldLoader* loader = 0;
        if (fieldName == "position")
        {
            hasPosition = true;
            loader = &fieldLoaders[0];
        }
        else
        {
            TypeSpec type(baseType, elsize, maxComponentIndex+1, semantics, fixedPoint);
            //tfm::printf("%s: type %s\n", fieldName, type);
            fields.push_back(GeomField(type, fieldName, npoints));
            fieldLoaders.push_back(PlyFieldLoader(fields.back()));
            loader = &fieldLoaders.back();
        }
        for (size_t j = eltBegin; j < eltEnd; ++j)
        {
            ply_set_read_cb(ply, "vertex", fieldInfo[j].plyName.c_str(),
                            &PlyFieldLoader::rplyCallback,
                            loader, fieldInfo[j].componentIndex);
        }
    }
    if (!hasPosition)
    {
        g_logger.error("No position property found in file %s", fileName);
        return false;
    }

    // All setup is done; read ply file using the callbacks
    if (!ply_read(ply))
        return false;

    offset = fieldLoaders[0].offset();
    return true;
}


//------------------------------------------------------------------------------
// Utilities for loading ply in "displaz-native" format

/// Find all elements with name "vertex_*"
bool findVertexElements(std::vector<p_ply_element>& vertexElements,
                        p_ply ply, size_t& npoints)
{
    int64_t np = -1;
    for (p_ply_element elem = ply_get_next_element(ply, NULL);
         elem != NULL; elem = ply_get_next_element(ply, elem))
    {
        const char* name = 0;
        long ninstances = 0;
        if (!ply_get_element_info(elem, &name, &ninstances))
            continue;
        if (strncmp(name, "vertex_", 7) == 0)
        {
            if (np == -1)
                np = ninstances;
            if (np != ninstances)
            {
                g_logger.error("Inconsistent number of points in \"vertex_*\" fields");
                return false;
            }
            vertexElements.push_back(elem);
        }
        else
        {
            g_logger.warning("Ignoring unrecogized ply element: %s", name);
        }
    }

    npoints = np;
    return true;
}


bool loadDisplazNativePly(QString fileName, p_ply ply,
                          std::vector<GeomField>& fields, V3d& offset,
                          size_t& npoints)
{
    std::vector<p_ply_element> vertexElements;
    if (!findVertexElements(vertexElements, ply, npoints))
        return false;

    // Map each vertex element to the associated displaz type
    std::vector<PlyFieldLoader> fieldLoaders;
    // Reserve to avoid reallocs which invalidate pointers to elements.
    fields.reserve(vertexElements.size());
    fieldLoaders.reserve(vertexElements.size());
    for (auto elem = vertexElements.begin(); elem != vertexElements.end(); ++elem)
    {
        const char* elemName = 0;
        ply_get_element_info(*elem, &elemName, 0);
        assert(elemName);

        // Figure out type of current element.  All properties of the element
        // should have the same type.
        TypeSpec::Type baseType = TypeSpec::Unknown;
        int elsize = 0;
        p_ply_property firstProp = ply_get_next_property(*elem, NULL);
        e_ply_type firstPropType = PLY_LIST;
        const char* firstPropName = 0;
        ply_get_property_info(firstProp, &firstPropName, &firstPropType, NULL, NULL);
        plyTypeToPointFieldType(firstPropType, baseType, elsize);

        // Determine semantics from first property.  Displaz-native storage
        // doesn't care about the rest of the property names (or perhaps it
        // should be super strict?)
        TypeSpec::Semantics semantics;
        if (strcmp(firstPropName, "x") == 0)
            semantics = TypeSpec::Vector;
        else if (strcmp(firstPropName, "r") == 0)
            semantics = TypeSpec::Color;
        else if (strcmp(firstPropName, "0") == 0)
            semantics = TypeSpec::Array;
        else
        {
            g_logger.error("Could not determine vector semantics for property %s.%s: expected property name x, r or 0", elemName, firstPropName);
            return false;
        }
        // Count properties
        int numProps = 0;
        for (p_ply_property prop = ply_get_next_property(*elem, NULL);
             prop != NULL; prop = ply_get_next_property(*elem, prop))
        {
            numProps += 1;
        }

        std::string fieldName = elemName + 7; // Strip off "vertex_" prefix
        if (fieldName == "position")
        {
            if (numProps != 3)
            {
                g_logger.error("position field must have three elements, found %d", numProps);
                return false;
            }
            // Force "vector float[3]" for position
            semantics = TypeSpec::Vector;
            elsize = 4;
            baseType = TypeSpec::Float;
        }

        // Create loader callback object
        TypeSpec type(baseType, elsize, numProps, semantics, false);
        fields.push_back(GeomField(type, fieldName, npoints));
        fieldLoaders.push_back(PlyFieldLoader(fields.back()));
        // Connect callbacks for each property
        int propIdx = 0;
        for (p_ply_property prop = ply_get_next_property(*elem, NULL);
             prop != NULL; prop = ply_get_next_property(*elem, prop))
        {
            e_ply_type propType;
            const char* propName = 0;
            ply_get_property_info(prop, &propName, &propType, NULL, NULL);
            ply_set_read_cb(ply, elemName, propName, &PlyFieldLoader::rplyCallback,
                            &fieldLoaders.back(), propIdx);
            propIdx += 1;
        }
        g_logger.info("%s: %s %s", fileName, type, fieldName);
    }

    // All setup is done; read ply file using the callbacks
    if (!ply_read(ply))
    {
        g_logger.error("Error in ply_read()");
        return false;
    }

    offset = fieldLoaders[0].offset();
    return true;
}


void logRplyError(p_ply ply, const char* message)
{
    g_logger.error("rply: %s", message);
}
