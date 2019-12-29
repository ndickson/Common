#pragma once

// This file contains structures, enumerations, and functions directly
// related to BMP image file format.

#include "../Types.h"

OUTER_NAMESPACE_BEGIN
namespace bmp {

using namespace COMMON_LIBRARY_NAMESPACE;

// These BMP file format structures have members NOT in aligned locations,
// so enabling struct packing is necessary, to avoid any padding bytes.
#pragma pack(push, 1)

constexpr static uint16 BMP_ID = uint16('B') | (uint16('M')<<8);

// This is at the very beginning of the file.
struct alignas(1) BMPFileHeader {
	uint16  bmpID;      // "BM" (BMP_ID), or possibly some other values in very old files
	uint32  fileSize;   // Full size of the file, including this header
	uint16  reserved0;
	uint16  reserved1;
	uint32  pixelDataOffset; // Offset within the file of the pixel data
};

// This immediately follows BMPFileHeader in the file.
// Common sizes of this are 40 bytes, 108 bytes (V4), and 124 bytes (V5)
struct alignas(1) DIBHeader {
	uint32  dibHeaderSize; // Usually 40 bytes for this structure, but first
	                       // 5 members (16 bytes) always present
	int32   imageWidth;    // Negative value indicates right-to-left
	int32   imageHeight;   // Negative value indicates top-to-bottom
	uint16  nImagePlanes;  // Must be 1
	uint16  bitsPerPixel;  // Usually 1, 4, 8, 16, 24, or 32

	uint32  compressionMethod;// Usually zero, meaning uncompressed
	uint32  sizeIfCompressed; // Can be zero if uncompressed
	int32   pixelsPerMetreX;  // 2835 is about 72 DPI
	int32   pixelsPerMetreY;  //
	uint32  nPaletteColours;  // Zero for 2^bitsPerPixel
	uint32  nImportantColours;// Usually zero
};

#pragma pack(pop)

} // namespace bmp
OUTER_NAMESPACE_END
