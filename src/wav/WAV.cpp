// This file contains definitions of functions for reading/writing WAV audio files.

#include "wav/WAV.h"
#include "wav/WAVFormat.h"
#include "File.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"

#include <limits>  // For std::numeric_limits
#include <utility> // For std::pair

OUTER_NAMESPACE_BEGIN
namespace wav {

// Default conversion works for conversion between float and double
// and for identical types.
template<typename FROM_T, typename TO_T>
INLINE void convertDataTo(const FROM_T from, TO_T& to) {
	to = TO_T(from);
}
template<> INLINE void convertDataTo(const int16 from, uint8& to) {
	uint8 temp = uint8((from >> 8) + 128);
	// Round up if closest and won't overflow
	to = temp + ((from & 0x80) != 0 && temp != 0xFF);
}
template<> INLINE void convertDataTo(const int32 from, uint8& to) {
	uint8 temp = uint8((from >> 24) + 128);
	// Round up if closest and won't overflow
	to = temp + ((from & (1<<23)) != 0 && temp != 0xFF);
}
template<> INLINE void convertDataTo(const int64 from, uint8& to) {
	uint8 temp = uint8((from >> 56) + 128);
	// Round up if closest and won't overflow
	to = temp + ((from & (1ULL<<55)) != 0 && temp != 0xFF);
}
template<> INLINE void convertDataTo(const float from, uint8& to) {
	// Round to closest
	// TODO: Should this be *127.5 and a different shift?
	int16 temp = int16((from * 128) + 128.5f);
	to = (temp <= 0) ? uint8(0) : ((temp >= 255) ? uint8(255) : uint8(temp));
}
template<> INLINE void convertDataTo(const double from, uint8& to) {
	// Round to closest
	// TODO: Should this be *127.5 and a different shift?
	int16 temp = int16((from * 128) + 128.5);
	to = (temp <= 0) ? uint8(0) : ((temp >= 255) ? uint8(255) : uint8(temp));
}
template<> INLINE void convertDataTo(const uint8 from, int16& to) {
	// Zero extend before subtracting 128
	to = (int16(uint16(from)) - 128) << 8;
}
template<> INLINE void convertDataTo(const int32 from, int16& to) {
	// Just shift
	int16 temp = int16(from>>16);
	// Round up if closest and won't overflow
	to = temp + ((from & (1<<15)) != 0 && temp != 0x7FFF);
}
template<> INLINE void convertDataTo(const int64 from, int16& to) {
	// Just shift
	int16 temp = int16(from>>48);
	// Round up if closest and won't overflow
	to = temp + ((from & (1ULL<<47)) != 0 && temp != 0x7FFF);
}
template<> INLINE void convertDataTo(const float from, int16& to) {
	// Round to closest
	// TODO: Should this be *32767.5 and a different shift?
	to = int16((from * 32768) + 0.5f);
}
template<> INLINE void convertDataTo(const double from, int16& to) {
	// Round to closest
	// TODO: Should this be *32767.5 and a different shift?
	to = int16((from * 32768) + 0.5);
}

template<> INLINE void convertDataTo(const uint8 from, float& to) {
	to = (float(from) - 128.0f) * (1.0f/128.0f);
}
template<> INLINE void convertDataTo(const int16 from, float& to) {
	to = float(from) * (1.0f/32768.0f);
}
template<> INLINE void convertDataTo(const int32 from, float& to) {
	to = float(from) * (1.0f/2147483648.0f);
}
template<> INLINE void convertDataTo(const int64 from, float& to) {
	to = float(from) * (1.0f/9223372036854775808.0f);
}
template<> INLINE void convertDataTo(const uint8 from, double& to) {
	to = (double(from) - 128.0) * (1.0/128.0);
}
template<> INLINE void convertDataTo(const int16 from, double& to) {
	to = double(from) * (1.0/32768.0);
}
template<> INLINE void convertDataTo(const int32 from, double& to) {
	to = double(from) * (1.0/2147483648.0);
}
template<> INLINE void convertDataTo(const int64 from, double& to) {
	to = double(from) * (1.0/9223372036854775808.0);
}

template<typename T>
void loadWAVData(
	const uint8* data,
	const size_t nBlocks,
	const size_t nChannels,
	const size_t bytesPerSample,
	const size_t validBitsPerSample,
	const bool isFloatDataFormat,
	Array<T>* tracks) {

	for (size_t channeli = 0; channeli < nChannels; ++channeli) {
		tracks[channeli].setSize(nBlocks);
	}

	if (isFloatDataFormat) {
		if (bytesPerSample == 4 && validBitsPerSample == 32) {
			const float* floatData = reinterpret_cast<const float*>(data);
			for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
				for (size_t channeli = 0; channeli < nChannels; ++channeli) {
					convertDataTo(*floatData, tracks[channeli][blocki]);
					++floatData;
				}
			}
		}
		else if (bytesPerSample == 8 && validBitsPerSample == 64) {
			const double* floatData = reinterpret_cast<const double*>(data);
			for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
				for (size_t channeli = 0; channeli < nChannels; ++channeli) {
					convertDataTo(*floatData, tracks[channeli][blocki]);
					++floatData;
				}
			}
		}
		else {
			// FIXME: Implement other floating-point cases!!!
		}
	}
	else {
		// Integer data
		if (bytesPerSample == 1) {
			// 8-bit integer data are considered unsigned
			if (validBitsPerSample >= 8) {
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						convertDataTo(*data, tracks[channeli][blocki]);
						++data;
					}
				}
			}
			else {
				// Keep high bits, as specified in the standard.
				const uint8 mask = uint8(0xFF<<(8-validBitsPerSample));
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						convertDataTo(*data & mask, tracks[channeli][blocki]);
						++data;
					}
				}
			}
		}
		else if (bytesPerSample == 2) {
			// 16-bit integer data are considered signed
			const int16* intData = reinterpret_cast<const int16*>(data);
			if (validBitsPerSample >= 16) {
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						convertDataTo(*intData, tracks[channeli][blocki]);
						++intData;
					}
				}
			}
			else {
				// Keep high bits, as specified in the standard.
				const int16 mask = int16(0xFFFF<<(16-validBitsPerSample));
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						convertDataTo(*intData & mask, tracks[channeli][blocki]);
						++intData;
					}
				}
			}
		}
		else if (bytesPerSample == 3) {
			// 24-bit integer data are considered signed
			if (validBitsPerSample >= 24) {
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						int32 value = int32((uint32(data[0])<<8) | (uint32(data[1])<<16) | (uint32(data[2])<<24));
						convertDataTo(value, tracks[channeli][blocki]);
						data += 3;
					}
				}
			}
			else {
				// Keep high bits, as specified in the standard.
				const int32 mask = int32(0xFFFFFFFF<<(32-validBitsPerSample));
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						int32 value = int32((uint32(data[0])<<8) | (uint32(data[1])<<16) | (uint32(data[2])<<24));
						convertDataTo(value & mask, tracks[channeli][blocki]);
						data += 3;
					}
				}
			}
		}
		else if (bytesPerSample == 4) {
			// 32-bit integer data are considered signed
			const int32* intData = reinterpret_cast<const int32*>(data);
			if (validBitsPerSample >= 32) {
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						convertDataTo(*intData, tracks[channeli][blocki]);
						++intData;
					}
				}
			}
			else {
				// Keep high bits, as specified in the standard.
				const int32 mask = int32(0xFFFFFFFF<<(32-validBitsPerSample));
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						convertDataTo(*intData & mask, tracks[channeli][blocki]);
						++intData;
					}
				}
			}
		}
		else {
			// High-precision integer data are considered signed
			if (validBitsPerSample >= 8*bytesPerSample || validBitsPerSample >= 64) {
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						int64 value = 0;
						// Start with the high order byte at the top
						for (size_t bytei = 0; bytei < bytesPerSample && bytei < 8; ++bytei) {
							value |= int64(uint64(data[bytesPerSample-1-bytei]) << (64-8*bytei));
						}
						if (bytesPerSample > 8 && (data[bytesPerSample-1-8] & 0x80)) {
							// Round up
							++value;
						}
						convertDataTo(value, tracks[channeli][blocki]);
						data += bytesPerSample;
					}
				}
			}
			else {
				// Keep high bits, as specified in the standard.
				const int64 mask = int64(0xFFFFFFFFFFFFFFFFULL<<(64-validBitsPerSample));
				for (size_t blocki = 0; blocki < nBlocks; ++blocki) {
					for (size_t channeli = 0; channeli < nChannels; ++channeli) {
						int64 value = 0;
						// Start with the high order byte at the top
						for (size_t bytei = 0; bytei < bytesPerSample && bytei < 8; ++bytei) {
							value |= int64(uint64(data[bytesPerSample-1-bytei]) << (64-8*bytei));
						}
						if (bytesPerSample > 8 && (data[bytesPerSample-1-8] & 0x80)) {
							// Round up
							++value;
						}
						convertDataTo(value & mask, tracks[channeli][blocki]);
						data += bytesPerSample;
					}
				}
			}
		}
	}
}

template<typename T>
bool ReadWAVFile(const char* filename, AudioTracks<T>& tracks) {
	tracks.nSamplesPerSecond = 0;
	tracks.tracks.setSize(0);

	Array<char> contents;
	ReadWholeFile(filename, contents);

	size_t size = contents.size();
	const uint8* data = reinterpret_cast<const uint8*>(contents.data());

	Array<std::pair<const uint8*,size_t>> deferredData;

	bool isFormatKnown = false;
	bool isFloatDataFormat = false;
	size_t bytesPerSample = 2;
	size_t validBitsPerSample = 16;
	size_t nChannels = 1;
	size_t maxSamplesPerChannel = std::numeric_limits<size_t>::max();

	// NOTE: There may be multiple RIFF chunks, but usually only one.
	while (size >= sizeof(FileHeader)) {
		const FileHeader* riffHeader = reinterpret_cast<const FileHeader*>(data);
		if (riffHeader->riffID != RIFF_ID || riffHeader->waveID != WAVE_ID) {
			// Invalid RIFF chunk
			return false;
		}
		size_t riffChunkSize = riffHeader->riffSize;
		if (riffChunkSize+(sizeof(FileHeader)-sizeof(FileHeader::waveID)) > size || riffChunkSize < sizeof(FileHeader::waveID)) {
			// RIFF chunk size either beyond end of file or before end of header
			return false;
		}

		// Keep track of the remaining data in the file, RIFF chunk, and subchunk,
		// as well as the current data pointer.
		riffChunkSize -= sizeof(FileHeader::waveID);
		size -= sizeof(FileHeader);
		data += sizeof(FileHeader);

		while (riffChunkSize >= sizeof(ChunkHeader)) {
			const ChunkHeader* chunkHeader = reinterpret_cast<const ChunkHeader*>(data);
			riffChunkSize -= sizeof(ChunkHeader);
			size -= sizeof(ChunkHeader);
			data += sizeof(ChunkHeader);

			const ChunkType chunkType = chunkHeader->chunkID;
			const size_t chunkSize = chunkHeader->chunkSize;

			// Chunks are required to be padded to even byte boundaries if odd size
			const size_t chunkSizePadded = chunkSize + (chunkSize & 1);
			if (chunkSizePadded > riffChunkSize) {
				// This chunk goes beyond the RIFF chunk limit
				return false;
			}

			if (chunkType == ChunkType::FORMAT) {
				if (chunkSize < sizeof(FormatChunk)) {
					// Chunk is too small for the basic format data
					return false;
				}

				const FormatChunk*const formatData = reinterpret_cast<const FormatChunk*>(data);

				// Just skip format chunks with no channels, or with block sizes
				// that aren't multiples of the number of channels,
				// or with block sizes of 0.
				// NOTE: nChannels and bytesPerSample MUST NOT be zero, else division by zero would occur below.
				if (formatData->nChannels != 0 && (formatData->blockSize % formatData->nChannels) == 0 && formatData->blockSize > 0) {
					nChannels = formatData->nChannels;
					bytesPerSample = formatData->blockSize / nChannels;
					validBitsPerSample = formatData->bitsPerSample;
					if (8*bytesPerSample < validBitsPerSample) {
						validBitsPerSample = 8*bytesPerSample;
					}
					isFloatDataFormat = (formatData->formatType == FormatType::FLOAT);
					tracks.nSamplesPerSecond = formatData->blocksPerSec;

					// There may be additional data in the format chunk.
					if (chunkSize >= sizeof(FormatChunk)+sizeof(FormatChunkExtension)) {
						const FormatChunkExtension*const extendedFormatData = reinterpret_cast<const FormatChunkExtension*>(formatData+1);
						if (extendedFormatData->extensionSize >= sizeof(FormatChunkExtension)-sizeof(FormatChunkExtension::extensionSize)) {
							if (extendedFormatData->validBitsPerSample < validBitsPerSample) {
								validBitsPerSample = extendedFormatData->validBitsPerSample;
							}
							if (formatData->formatType == FormatType::EXTENSIBLE) {
								isFloatDataFormat = (extendedFormatData->formatType == FormatType::FLOAT);
							}
						}
					}

					isFormatKnown = true;

					if (deferredData.size() > 0) {
						// Process deferred data
						for (size_t i = 0, n = deferredData.size(); i < n; ++i) {
							const size_t dataSize = deferredData[i].second;
							size_t nBlocks = dataSize / (nChannels*bytesPerSample);
							if (maxSamplesPerChannel < nBlocks) {
								nBlocks = maxSamplesPerChannel;
							}
							if (nBlocks > 0) {
								const size_t firstTrack = tracks.tracks.size();
								tracks.tracks.setSize(firstTrack+nChannels);
								loadWAVData(deferredData[i].first, nBlocks, nChannels, bytesPerSample, validBitsPerSample, isFloatDataFormat, tracks.tracks.data()+firstTrack);
							}
						}
						deferredData.setSize(0);
					}
				}
			}
			else if (chunkType == ChunkType::FACT) {
				if (chunkSize < sizeof(FactChunk)) {
					// Chunk is too small for the "fact" chunk
					return false;
				}

				const FactChunk*const factData = reinterpret_cast<const FactChunk*>(data);
				maxSamplesPerChannel = factData->samplesPerChannel;
			}
			else if (chunkType == ChunkType::DATA) {
				// The data immediately follows its chunk header.

				// If there hasn't been a format chunk yet, defer processing the data chunk.
				if (!isFormatKnown) {
					deferredData.append(std::make_pair(data, chunkSize));
					continue;
				}

				size_t nBlocks = chunkSize / (nChannels*bytesPerSample);
				if (maxSamplesPerChannel < nBlocks) {
					nBlocks = maxSamplesPerChannel;
				}
				if (nBlocks > 0) {
					const size_t firstTrack = tracks.tracks.size();
					tracks.tracks.setSize(firstTrack+nChannels);
					loadWAVData(data, nBlocks, nChannels, bytesPerSample, validBitsPerSample, isFloatDataFormat, tracks.tracks.data()+firstTrack);
				}
			}
			// All other chunk types are skipped

			riffChunkSize -= chunkSizePadded;
			size -= chunkSizePadded;
			data += chunkSizePadded;
		}
		if (riffChunkSize > 0) {
			// Not the end of the RIFF chunk, but not enough space for even a chunk header.
			return false;
		}
	}

	if (contents.size() == 0 || (size > 0 && size < sizeof(FileHeader))) {
		// Incomplete file header
		return false;
	}

	// Successfully completed
	return tracks.tracks.size() > 0;
}

template bool ReadWAVFile<uint8>(const char* filename, AudioTracks<uint8>& tracks);
template bool ReadWAVFile<int16>(const char* filename, AudioTracks<int16>& tracks);
template bool ReadWAVFile<float>(const char* filename, AudioTracks<float>& tracks);
template bool ReadWAVFile<double>(const char* filename, AudioTracks<double>& tracks);

template<typename T>
bool WriteWAVFile(const char* filename, const AudioTracks<T>& tracks, size_t outputBitsPerSample) {
	// FIXME: Implement this!!!
}

template bool WriteWAVFile<uint8>(const char* filename, const AudioTracks<uint8>& tracks, size_t outputBitsPerSample);
template bool WriteWAVFile<int16>(const char* filename, const AudioTracks<int16>& tracks, size_t outputBitsPerSample);
template bool WriteWAVFile<float>(const char* filename, const AudioTracks<float>& tracks, size_t outputBitsPerSample);
template bool WriteWAVFile<double>(const char* filename, const AudioTracks<double>& tracks, size_t outputBitsPerSample);

} // namespace wav
OUTER_NAMESPACE_END
