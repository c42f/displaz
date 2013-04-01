#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#ifdef _WIN32
// FIXME
#include <ImathVec.h>
#include <ImathBox.h>
#include <ImathColor.h>
#include <ImathMatrix.h>
#else
#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathBox.h>
#include <OpenEXR/ImathColor.h>
#include <OpenEXR/ImathMatrix.h>
#endif

using Imath::V3d;
using Imath::V3f;
using Imath::V2f;
using Imath::C3f;


#endif // UTIL_H_INCLUDED
