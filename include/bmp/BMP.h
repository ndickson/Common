#pragma once

// This file contains declarations of functions for reading and writing
// image data from BMP files into a simple in-memory format.
// Definitions of the functions are in BMP.cpp

#include "../Types.h"
#include "../Array.h"

OUTER_NAMESPACE_BEGIN
namespace bmp {

using namespace COMMON_LIBRARY_NAMESPACE;

COMMON_LIBRARY_EXPORTED bool ReadBMPFile(const char* filename, Array<uint32>& pixels, size_t& width, size_t& height, bool& hasAlpha);

COMMON_LIBRARY_EXPORTED bool WriteBMPFile(const char* filename, const uint32* pixels, size_t width, size_t height, bool saveAlpha);

} // namespace bmp
OUTER_NAMESPACE_END
