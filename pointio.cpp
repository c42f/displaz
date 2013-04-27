#include <algorithm>
#include <memory>
#include <vector>
#include <stdexcept>

#include "tinyformat.h"

/// An interface for loading point data which supports generic attributes
///
/// Aims for the interface
///
/// * Convenient access to arbitrarily named point attributes
///   - Ultimate efficiency for statically known attributes
///   - Ability to use attributes which are only known at runtime (TODO)
/// * Support out of core iteration over large files
/// * Zero-copy support for memory buffers/mmaped files as backend
///
/// The implementation attempts to make arbitrary named attributes almost as
/// nice to use as an array of structs (we can't just use an array of structs
/// because this would limit us to a rigid set of attributes known at compile
/// time).
///

namespace ptio
{

class PointInput;

class AttrBase
{
    public:
        inline AttrBase(const char* data, int stride, PointInput* input);

        inline AttrBase(const AttrBase& other);

        inline AttrBase& operator=(AttrBase& other);

        inline ~AttrBase();

    protected:
        friend class Iterator;

        /// Storage buffer, managed externally
        const char* m_data;
        /// Stride between elements, in bytes
        int m_stride;
        /// Associated input class (manages buffers etc)
        const PointInput* m_input;
};


/// Encode the type of the data in the attribute type
template<typename T>
class Attr : public AttrBase
{
    public:
        typedef T attr_type;

        Attr(const char* data, int stride, PointInput* input)
            : AttrBase(data, stride, input)
        { }
};


class Iterator
{
    public:
        Iterator()
            : m_pos(-1), m_nextChunk(-1), m_input(0)
        { }

        Iterator(size_t pos, size_t nextChunk, PointInput* input)
            : m_pos(pos),
            m_nextChunk(nextChunk),
            m_input(input)
        { }

        /// Iterator operations
        bool operator!=(const Iterator& rhs) const
        {
            return m_pos != rhs.m_pos;
        }

        inline Iterator& operator++();

        /// Attribute indexing
        template<typename T>
        T operator[](const Attr<T>& attr) const
        {
            return *reinterpret_cast<const T*>(attr.m_data + m_pos*attr.m_stride);
        }

        /// Indexing for array attributes
        template<typename T>
        const T* operator[](const Attr<T[]>& attr) const
        {
            return reinterpret_cast<const T*>(attr.m_data + m_pos*attr.m_stride);
        }

    private:
        size_t m_pos;
        size_t m_nextChunk;
        /// Associated input class (manages buffers etc)
        PointInput* m_input;
};


class PointInput
{
    public:
        PointInput()
        {
            static double posdata[] = {1,2,3,4,5,6,7,8,9};
            m_attrData.push_back(AttrData("x", reinterpret_cast<const char*>(&posdata[0]), 3*sizeof(double)));
            m_attrData.push_back(AttrData("y", reinterpret_cast<const char*>(&posdata[1]), 3*sizeof(double)));
            m_attrData.push_back(AttrData("z", reinterpret_cast<const char*>(&posdata[2]), 3*sizeof(double)));
            m_attrData.push_back(AttrData("position", reinterpret_cast<const char*>(&posdata[0]), 3*sizeof(double)));
            static uint16_t intensdata[] = {0,42,100};
            m_attrData.push_back(AttrData("intensity", reinterpret_cast<const char*>(&intensdata[0]), sizeof(uint16_t)));
        }

        virtual ~PointInput() {}

        static std::unique_ptr<PointInput> create(const std::string& path)
        {
            return std::unique_ptr<PointInput>(new PointInput());
        }

        template<typename T>
        Attr<T> findAttr(const std::string& name)
        {
            for (size_t i = 0; i < m_attrData.size(); ++i)
            {
                if (m_attrData[i].name == name)
                    return Attr<T>(m_attrData[i].data, m_attrData[i].stride, this);
            }
            throw std::runtime_error("Attribute not found");
        }

        Iterator begin()
        {
            return Iterator(0, 4, this);
        }

        Iterator end()
        {
            return Iterator(3, 4, this);
        }

        virtual void nextChunk(size_t& pos, size_t& chunkEnd)
        {
            // Update pointer for each attribute
        }

    private:
        friend class Iterator;
        friend class AttrBase;

        void registerAttr(AttrBase* attr) const
        {
            m_attrs.push_back(attr);
        }

        void unregisterAttr(AttrBase* attr) const
        {
            auto iter = std::find(m_attrs.begin(), m_attrs.end(), attr);
            if (iter != m_attrs.end())
                m_attrs.erase(iter);
            else
                throw std::runtime_error("Point attribute not found in unregistration");
        }

        struct AttrData
        {
            std::string name;
            const char* data;
            int stride;
            // int count;
            // TypeDesc type;

            AttrData(const std::string& name, const char* data, int stride)
                : name(name), data(data), stride(stride) { }
        };

        std::vector<AttrData> m_attrData;
        mutable std::vector<AttrBase*> m_attrs;
};


AttrBase::AttrBase(const char* data, int stride, PointInput* input)
    : m_data(data), m_stride(stride), m_input(input)
{
    m_input->registerAttr(this);
}

AttrBase::AttrBase(const AttrBase& other)
    : m_data(other.m_data), m_stride(other.m_stride), m_input(other.m_input)
{
    m_input->registerAttr(this);
}

AttrBase& AttrBase::operator=(AttrBase& other)
{
    m_input->unregisterAttr(this);
    m_data = other.m_data;
    m_stride = other.m_stride;
    m_input = other.m_input;
    m_input->registerAttr(this);
    return *this;
}

AttrBase::~AttrBase()
{
    m_input->unregisterAttr(this);
}


Iterator& Iterator::operator++()
{
    ++m_pos;
    if (m_pos >= m_nextChunk)
        m_input->nextChunk(m_pos, m_nextChunk);
    return *this;
}


} // namespace ptio


int main()
{
    using namespace ptio;
    std::unique_ptr<PointInput> in = PointInput::create("test");

    Attr<double> x = in->findAttr<double>("x");
    Attr<double> y = in->findAttr<double>("y");
    Attr<double> z = in->findAttr<double>("z");
    Attr<double[]> pos = in->findAttr<double[]>("position");
    Attr<uint16_t> intensity = in->findAttr<uint16_t>("intensity");

    for (Iterator pt = in->begin(); pt != in->end(); ++pt)
    {
        tfm::printf("pos = (%f,%f,%f)\n", pt[x], pt[y], pt[z]);
        const double* p = pt[pos];
        tfm::printf("pos2 = (%f,%f,%f), intensity = %d\n", p[0], p[1], p[2], pt[intensity]);
    }

    return 0;
}

