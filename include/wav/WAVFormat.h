#pragma once

#include "Types.h"

OUTER_NAMESPACE_BEGIN
namespace wav {

using namespace Common;

constexpr static uint32 RIFF_ID = 'R' | (uint32('I')<<8) | (uint32('F')<<16) | (uint32('F')<<24);
constexpr static uint32 WAVE_ID = 'W' | (uint32('A')<<8) | (uint32('V')<<16) | (uint32('E')<<24);

struct FileHeader {
	uint32  riffID;     // "RIFF" (RIFF_ID)
	uint32  riffSize;   // 4 bytes following this + size of all other chunks in the file
	uint32  waveID;     // "WAVE" (WAVE_ID)
};

enum class ChunkType : uint32 {
	// FormatChunkHeader
	FORMAT = 'f' | (uint32('m')<<8) | (uint32('t')<<16) | (uint32(' ')<<24),

	// FactChunkHeader
	FACT   = 'f' | (uint32('a')<<8) | (uint32('c')<<16) | (uint32('t')<<24),

	// Sample data immediately following ChunkHeader
	DATA   = 'd' | (uint32('a')<<8) | (uint32('t')<<16) | (uint32('a')<<24)
};

struct ChunkHeader {
	ChunkType   chunkID;
	uint32      chunkSize;  // Number of bytes in this chunk, excluding ChunkHeader
};

enum class FormatType : uint16 {
	PCM         = 0x0001,   // 8-bit unsigned, or likely 16-bit, 20-bit, or 24-bit signed integer samples
	FLOAT       = 0x0003,   // 16-bit, 32-bit, or 64-bit floating-point samples
	ALAW        = 0x0006,
	MULAW       = 0x0007,
	EXTENSIBLE  = 0xFFFE
};

// chunkID is FORMAT
// chunkSize should be 16, 18, or 40
struct FormatChunk : public ChunkHeader {
	FormatType formatType;  // If EXTENSIBLE, the FormatChunkExtension formatType has the format type
	uint16  nChannels;      // 1 for mono; 2 for stereo; up to 18 defined speaker locations
	uint32  blocksPerSec;   // Number of blocks per second (e.g. 44100 or 48000)
	uint32  bytesPerSec;    // blocksPerSec*blockSize (or average bytes/sec if compressed format)
	uint16  blockSize;      // nChannels*bitsPerSample/8
	uint16  bitsPerSample;  // 8 for 8-bit samples; 16 for 16-bit samples
};

struct FormatChunkExtension {
	uint16  extensionSize;      // 0 or 22 bytes in the extension following this
	uint16  validBitsPerSample; // Number of bits in each sample that actually contain data
	uint32  channelMask;        // Indication of which speaker locations have channels
	FormatType formatType;      // 1 for PCM; else some other type
	uint16  guid[7];            // 14 bytes of fixed values
};

// chunkID is FACT
// chunkSize should be 4
struct FactChunk : public ChunkHeader {
	uint32  samplesPerChannel;  
};

} // namespace wav
OUTER_NAMESPACE_END
