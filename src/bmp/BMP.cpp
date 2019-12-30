// This file contains definitions of functions for reading and writing
// image data from BMP format files into a simple in-memory format.
// Declarations of the functions are in BMP.h

#include "bmp/BMP.h"
#include "bmp/BMPFormat.h"
#include "File.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"

OUTER_NAMESPACE_BEGIN
namespace bmp {

bool ReadBMPFile(const char* filename, Array<uint32>& pixels, size_t& width, size_t& height, bool& hasAlpha) {
	pixels.setSize(0);
	width = 0;
	height = 0;
	hasAlpha = false;

	ReadFileHandle file = OpenFileRead(filename);
	if (file.isClear()) {
		return false;
	}

	uint64 fileSize = GetFileSize(file);
	if (fileSize < sizeof(BMPFileHeader) + sizeof(DIBHeader)) {
		return false;
	}

	BMPFileHeader fileHeader;
	static_assert(sizeof(BMPFileHeader) == 14, "There should be no padding bytes in BMPFileHeader!");
	size_t numBytesRead = ReadFile(file, &fileHeader, sizeof(BMPFileHeader));
	if (numBytesRead != sizeof(BMPFileHeader)) {
		return false;
	}
	if (fileHeader.bmpID != BMP_ID || fileHeader.fileSize > fileSize ||
		fileHeader.pixelDataOffset > fileHeader.fileSize ||
		fileHeader.pixelDataOffset < sizeof(BMPFileHeader) + sizeof(DIBHeader)
	) {
		return false;
	}

	DIBHeader dibHeader;
	static_assert(sizeof(DIBHeader) == 40, "There should be no padding bytes in DIBHeader!");
	numBytesRead = ReadFile(file, &dibHeader, sizeof(DIBHeader));
	if (numBytesRead != sizeof(DIBHeader) || dibHeader.dibHeaderSize < sizeof(DIBHeader)) {
		return false;
	}
	// Currently, only uncompressed, 24-bit or 32-bit bitmap images, with a single image plane are supported.
	if (dibHeader.compressionMethod != 0 ||
		(dibHeader.bitsPerPixel != 24 && dibHeader.bitsPerPixel != 32) ||
		dibHeader.nImagePlanes != 1
	) {
		return false;
	}

	int32 fileWidth = dibHeader.imageWidth;
	int32 fileHeight = dibHeader.imageHeight;
	const bool reverseHorizontal = (fileWidth < 0);
	const bool reverseVertical = (fileHeight < 0);
	fileWidth = reverseHorizontal ? -fileWidth : fileWidth;
	fileHeight = reverseVertical ? -fileHeight : fileHeight;

	const bool fileHasAlpha = (dibHeader.bitsPerPixel == 32);
	const size_t bytesPerPixel = 3 + size_t(fileHasAlpha);

	size_t scanlineBytes = bytesPerPixel*size_t(fileWidth);
	if (!fileHasAlpha) {
		// Round up to multiple of 4 bytes.
		scanlineBytes = (scanlineBytes + 3) & ~size_t(3);
	}

	const uint64 pixelDataBytes = scanlineBytes*uint64(fileHeight);
	if (fileHeader.pixelDataOffset + pixelDataBytes < fileHeader.fileSize) {
		return false;
	}

	bool success = SetFileOffset(file, uint64(fileHeader.pixelDataOffset), FileOffsetType::ABSOLUTE);

	pixels.setSize(size_t(fileWidth)*fileHeight);
	if ((bytesPerPixel == 4) && !reverseVertical && !reverseHorizontal) {
		// Data in memory lines up with data in file, so just do one read.
		numBytesRead = ReadFile(file, pixels.data(), pixelDataBytes);
		if (numBytesRead != pixelDataBytes) {
			return false;
		}
	}
	else {
		Array<uint8> scanline;
		scanline.setSize(scanlineBytes);

		for (size_t line = 0; line < fileHeight; ++line) {
			// Read a scan line at a time
			numBytesRead = ReadFile(file, scanline.data(), scanlineBytes);
			if (numBytesRead != scanlineBytes) {
				pixels.setSize(0);
				return false;
			}

			uint64 outputLineStart = (reverseVertical ? (size_t(fileHeight)-1-line) : line)*fileWidth;
			const uint8* fileData = scanline.data();
			for (size_t pixeli = 0; pixeli < fileWidth; ++pixeli) {
				const uint32 pixelValue =
					uint32(fileData[0]) |
					(uint32(fileData[1])<<8) |
					(uint32(fileData[2])<<16) |
					(fileHasAlpha ? (uint32(fileData[3])<<24) : 0xFF000000);
				fileData += bytesPerPixel;

				pixels[outputLineStart + (reverseHorizontal ? (size_t(fileWidth)-1-pixeli) : pixeli)] = pixelValue;
			}
		}
	}

	width = fileWidth;
	height = fileHeight;
	hasAlpha = fileHasAlpha;
	return true;
}

bool WriteBMPFile(const char* filename, const uint32* pixels, size_t width, size_t height, bool saveAlpha) {
	// Width and height can't be larger than (2^31 - 1), since they're stored in a signed int32 in the file.
	if (width >= 0x7FFFFFFF || height >= 0x7FFFFFFF) {
		return false;
	}
	size_t bytesPerPixel = 3 + size_t(saveAlpha);
	uint64 scanlineBytes = uint64(width)*bytesPerPixel;
	if (!saveAlpha) {
		// Round up to multiple of 4 bytes.
		scanlineBytes = (scanlineBytes + 3) & ~size_t(3);
	}
	// Carefully avoid overflow by checking scanlineBytes separately from fileSize.
	if (scanlineBytes >= 0x80000000) {
		return false;
	}

	uint64 pixelDataBytes = scanlineBytes*height;
	uint64 fileSize = pixelDataBytes + sizeof(BMPFileHeader) + sizeof(DIBHeader);
	// File size must be able to fit into a uint32 in the file.
	if (fileSize > 0xFFFFFFFF) {
		return false;
	}

	WriteFileHandle file = CreateFile(filename);
	if (file.isClear()) {
		return false;
	}

	BMPFileHeader fileHeader;
	fileHeader.bmpID = BMP_ID;
	fileHeader.fileSize = uint32(fileSize);
	fileHeader.reserved0 = uint16('A') | (uint16('T')<<8);
	fileHeader.reserved1 = uint16('Q') | (uint16('F')<<8);
	fileHeader.pixelDataOffset = sizeof(BMPFileHeader) + sizeof(DIBHeader);
	static_assert(sizeof(BMPFileHeader) == 14, "There should be no padding bytes in BMPFileHeader!");
	size_t numBytesWritten = WriteFile(file, &fileHeader, sizeof(BMPFileHeader));
	if (numBytesWritten != sizeof(BMPFileHeader)) {
		return false;
	}

	DIBHeader dibHeader;
	static_assert(sizeof(DIBHeader) == 40, "There should be no padding bytes in DIBHeader!");
	dibHeader.dibHeaderSize = sizeof(DIBHeader);
	dibHeader.imageWidth = int32(width);
	dibHeader.imageHeight = int32(height);
	dibHeader.nImagePlanes = 1;
	dibHeader.bitsPerPixel = uint16(bytesPerPixel*8);
	dibHeader.compressionMethod = 0;
	dibHeader.sizeIfCompressed = 0;
	dibHeader.pixelsPerMetreX = 2835;
	dibHeader.pixelsPerMetreY = 2835;
	dibHeader.nPaletteColours = 0;
	dibHeader.nImportantColours = 0;
	numBytesWritten = WriteFile(file, &dibHeader, sizeof(DIBHeader));
	if (numBytesWritten != sizeof(DIBHeader)) {
		return false;
	}

	if (bytesPerPixel == 4) {
		// Data in memory lines up with data in file, so just do one write.
		numBytesWritten = WriteFile(file, pixels, pixelDataBytes);
		if (numBytesWritten != pixelDataBytes) {
			return false;
		}
		return true;
	}

	// This codepath is only for when saveAlpha is false and bytesPerPixel is 3.
	Array<uint8> scanline;
	scanline.setSize(scanlineBytes);

	for (size_t line = 0; line < height; ++line) {
		uint8* fileData = scanline.data();
		for (size_t pixeli = 0; pixeli < width; ++pixeli) {
			const uint32 pixelValue = *pixels;
			++pixels;
			fileData[0] = uint8(pixelValue);
			fileData[1] = uint8(pixelValue>>8);
			fileData[2] = uint8(pixelValue>>16);
			fileData += bytesPerPixel;
		}

		// Write a scan line at a time
		numBytesWritten = WriteFile(file, scanline.data(), scanlineBytes);
		if (numBytesWritten != scanlineBytes) {
			return false;
		}
	}
	return true;
}

} // namespace bmp
OUTER_NAMESPACE_END
