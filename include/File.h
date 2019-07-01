#pragma once

// This file contains declarations of functions for reading/writing files.

#include "Types.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

bool ReadWholeFile(const char* filename, Array<char>& contents);
bool WriteWholeFile(const char* filename, const char* contents, size_t length);

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
