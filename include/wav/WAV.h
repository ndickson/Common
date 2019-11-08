#pragma once

// This file contains declarations of functions for reading and writing
// audio track data from WAV format files into a simple in-memory format.
// Definitions of the functions are in WAV.cpp

#include "Types.h"
#include "Array.h"

OUTER_NAMESPACE_BEGIN
namespace wav {

using namespace Common;

template<typename T>
struct AudioTracks {
	uint64 nSamplesPerSecond;
	BufArray<Array<T>,2> tracks;
};

// Instantiated for uint8, int16, float, and double.
template<typename T>
bool ReadWAVFile(const char* filename, AudioTracks<T>& tracks);

extern template COMMON_LIBRARY_EXPORTED bool ReadWAVFile<uint8>(const char* filename, AudioTracks<uint8>& tracks);
extern template COMMON_LIBRARY_EXPORTED bool ReadWAVFile<int16>(const char* filename, AudioTracks<int16>& tracks);
extern template COMMON_LIBRARY_EXPORTED bool ReadWAVFile<float>(const char* filename, AudioTracks<float>& tracks);
extern template COMMON_LIBRARY_EXPORTED bool ReadWAVFile<double>(const char* filename, AudioTracks<double>& tracks);

// Instantiated for uint8, int16, float, and double.
// outputBitsPerSample 8 or less will use uint8, up to 16 will use int16,
// up to 24 will use int24, up to 31 will use int32, 32 will use float, 64 will use double.
template<typename T>
bool WriteWAVFile(const char* filename, const AudioTracks<T>& tracks, size_t outputBitsPerSample);

extern template COMMON_LIBRARY_EXPORTED bool WriteWAVFile<uint8>(const char* filename, const AudioTracks<uint8>& tracks, size_t outputBitsPerSample);
extern template COMMON_LIBRARY_EXPORTED bool WriteWAVFile<int16>(const char* filename, const AudioTracks<int16>& tracks, size_t outputBitsPerSample);
extern template COMMON_LIBRARY_EXPORTED bool WriteWAVFile<float>(const char* filename, const AudioTracks<float>& tracks, size_t outputBitsPerSample);
extern template COMMON_LIBRARY_EXPORTED bool WriteWAVFile<double>(const char* filename, const AudioTracks<double>& tracks, size_t outputBitsPerSample);

} // namespace wav
OUTER_NAMESPACE_END
