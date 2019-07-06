#pragma once

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

extern template bool ReadWAVFile<uint8>(const char* filename, AudioTracks<uint8>& tracks);
extern template bool ReadWAVFile<int16>(const char* filename, AudioTracks<int16>& tracks);
extern template bool ReadWAVFile<float>(const char* filename, AudioTracks<float>& tracks);
extern template bool ReadWAVFile<double>(const char* filename, AudioTracks<double>& tracks);

// Instantiated for uint8, int16, float, and double.
// outputBitsPerSample 8 or less will use uint8, up to 16 will use int16,
// up to 24 will use int24, up to 31 will use int32, 32 will use float, 64 will use double.
template<typename T>
bool WriteWAVFile(const char* filename, const AudioTracks<T>& tracks, size_t outputBitsPerSample);

extern template bool WriteWAVFile<uint8>(const char* filename, const AudioTracks<uint8>& tracks, size_t outputBitsPerSample);
extern template bool WriteWAVFile<int16>(const char* filename, const AudioTracks<int16>& tracks, size_t outputBitsPerSample);
extern template bool WriteWAVFile<float>(const char* filename, const AudioTracks<float>& tracks, size_t outputBitsPerSample);
extern template bool WriteWAVFile<double>(const char* filename, const AudioTracks<double>& tracks, size_t outputBitsPerSample);

} // namespace wav
OUTER_NAMESPACE_END
