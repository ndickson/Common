// This file contains definitions of functions for reading and writing
// audio track data from WAV format files into a simple in-memory format.
// Declarations of the functions are in WAV.h

#include "wav/WAV.h"
#include "wav/WAVFormat.h"
#include "File.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"

#include <limits>  // For std::numeric_limits
#include <memory>  // For std::unique_ptr
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
template<> INLINE void convertDataTo(const uint8 from, int32& to) {
	// Zero extend before subtracting 128
	to = (int32(uint32(from)) - 128) << 24;
}
template<> INLINE void convertDataTo(const int16 from, int32& to) {
	// Just shift
	to = int32(from) << 16;
}
template<> INLINE void convertDataTo(const int64 from, int32& to) {
	// Just shift
	int32 temp = int32(from>>32);
	// Round up if closest and won't overflow
	to = temp + ((from & (1ULL<<31)) != 0 && temp != 0x7FFFFFFF);
}
template<> INLINE void convertDataTo(const float from, int32& to) {
	// Round to closest
	// TODO: Should this be *2147483647.5 and a different shift?
	to = int32((from * 2147483648) + 0.5f);
}
template<> INLINE void convertDataTo(const double from, int32& to) {
	// Round to closest
	// TODO: Should this be *2147483647.5 and a different shift?
	to = int32((from * 2147483648) + 0.5);
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
	bool success = ReadWholeFile(filename, contents);
	if (!success) {
		return false;
	}

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

template<typename FROM_T,typename TO_T>
void interleaveTrackData(const Array<Array<FROM_T>>& tracks, const size_t nSamplesPerTrack, TO_T* toData) {
	const size_t nTracks = tracks.size();
	if (nTracks == 1) {
		const Array<FROM_T>& track = tracks[0];
		for (size_t i = 0; i < nSamplesPerTrack; ++i) {
			convertDataTo(track[i], toData[i]);
		}
	}
	else {
		for (size_t blocki = 0; blocki < nSamplesPerTrack; ++blocki) {
			for (size_t tracki = 0; tracki < nTracks; ++tracki) {
				const Array<FROM_T>& track = tracks[tracki];
				if (blocki < track.size()) {
					convertDataTo(track[blocki], *toData);
				}
				else {
					convertDataTo(0.0f, *toData);
				}
				++toData;
			}
		}
	}
}

template<typename T>
bool WriteWAVFile(const char* filename, const AudioTracks<T>& tracks, size_t outputBitsPerSample) {
	// FormatChunk requires the number of blocks per second to fit into a uint32
	if (tracks.nSamplesPerSecond > std::numeric_limits<uint32>::max()) {
		return false;
	}

	// Determine output format
	bool isOutputFloatType = (outputBitsPerSample >= 32);
	size_t outputBytesPerSample;
	if (isOutputFloatType) {
		outputBytesPerSample = (outputBitsPerSample > 32) ? 8 : 4;
	}
	else {
		outputBytesPerSample = (outputBitsPerSample+7)/8;
		if (outputBytesPerSample < 1) {
			outputBytesPerSample = 1;
		}
		else if (outputBytesPerSample > 4) {
			outputBytesPerSample = 4;
		}
	}

	// Determine sample count
	const size_t nTracks = tracks.tracks.size();
	size_t nSamplesPerTrack = 0;
	for (size_t tracki = 0; tracki < nTracks; ++tracki) {
		const size_t currentSamples = tracks.tracks[tracki].size();
		if (currentSamples > nSamplesPerTrack) {
			nSamplesPerTrack = currentSamples;
		}
	}

	// FormatChunk requires the number of bytes per second to fit into a uint32
	size_t outputBytesPerSecond = tracks.nSamplesPerSecond*nTracks*outputBytesPerSample;
	if (outputBytesPerSecond > std::numeric_limits<uint32>::max()) {
		return false;
	}

	const size_t sampleDataSize = nTracks*nSamplesPerTrack*outputBytesPerSample;
	// Chunks are always padded to an even number of bytes
	const size_t sampleDataSizePadded = sampleDataSize + (sampleDataSize & 1);

	// The total file size (minus the first 8 bytes) must fit into a uint32
	const size_t size = sizeof(FileHeader) + 3*sizeof(ChunkHeader) + sizeof(uint32) + sizeof(float) + sizeof(FormatChunk) + sampleDataSizePadded;
	if (size-2*sizeof(uint32) > std::numeric_limits<uint32>::max() || nTracks*outputBytesPerSample > std::numeric_limits<uint16>::max()) {
		// TODO: Write multiple full RIFF chunks to represent more data.
		return false;
	}

	// For simplicity, write contents to memory buffer before writing to file
	char*const contentStart = new char[size];
	char* contents = contentStart;
	std::unique_ptr<char[]> contentsDeleter(contents);

	FileHeader*const fileHeader = reinterpret_cast<FileHeader*>(contents);
	fileHeader->riffID = RIFF_ID;
	fileHeader->riffSize = uint32(size - 2*sizeof(uint32));
	fileHeader->waveID = WAVE_ID;
	contents += sizeof(FileHeader);

	// Chunk indicating this code generated the WAV file, plus a 32-bit version number
	// whose bytes are not symmetrical and a float with value 1.0f, to record
	// endianness.
	ChunkHeader*const signatureChunkHeader = reinterpret_cast<ChunkHeader*>(contents);
	*reinterpret_cast<uint32*>(&signatureChunkHeader->chunkID) =
		uint32('A') | (uint32('T')<<8) | (uint32('Q')<<16) | (uint32('F')<<24);
	signatureChunkHeader->chunkSize = uint32(sizeof(uint64));
	contents += sizeof(ChunkHeader);

	uint32*const signatureInt = reinterpret_cast<uint32*>(contents);
	*signatureInt = 1;
	contents += sizeof(uint32);
	float*const signatureFloat = reinterpret_cast<float*>(contents);
	*signatureFloat = 1.0f;
	contents += sizeof(float);

	ChunkHeader*const formatChunkHeader = reinterpret_cast<ChunkHeader*>(contents);
	formatChunkHeader->chunkID = ChunkType::FORMAT;
	formatChunkHeader->chunkSize = uint32(sizeof(FormatChunk));
	contents += sizeof(ChunkHeader);

	FormatChunk*const formatChunk = reinterpret_cast<FormatChunk*>(contents);
	formatChunk->formatType = isOutputFloatType ? FormatType::FLOAT : FormatType::PCM;
	formatChunk->nChannels = uint16(nTracks);
	formatChunk->blocksPerSec = uint32(tracks.nSamplesPerSecond);
	formatChunk->bytesPerSec = uint32(outputBytesPerSecond);
	formatChunk->blockSize = uint16(nTracks*outputBytesPerSample);
	formatChunk->bitsPerSample = uint16(outputBytesPerSample*8);
	contents += sizeof(FormatChunk);

	ChunkHeader*const dataChunkHeader = reinterpret_cast<ChunkHeader*>(contents);
	dataChunkHeader->chunkID = ChunkType::DATA;
	dataChunkHeader->chunkSize = uint32(sampleDataSize);
	contents += sizeof(ChunkHeader);

	if (isOutputFloatType) {
		if (outputBytesPerSample == 4) {
			float* data = reinterpret_cast<float*>(contents);
			interleaveTrackData(tracks.tracks, nSamplesPerTrack, data);
		}
		else {
			double* data = reinterpret_cast<double*>(contents);
			interleaveTrackData(tracks.tracks, nSamplesPerTrack, data);
		}
	}
	else if (outputBytesPerSample == 1) {
		uint8* data = reinterpret_cast<uint8*>(contents);
		interleaveTrackData(tracks.tracks, nSamplesPerTrack, data);
	}
	else if (outputBytesPerSample == 2) {
		int16* data = reinterpret_cast<int16*>(contents);
		interleaveTrackData(tracks.tracks, nSamplesPerTrack, data);
	}
	else if (outputBytesPerSample == 3) {
		// Awkward case: 24-bit signed integers
		uint8* data = reinterpret_cast<uint8*>(contents);
		if (nTracks == 1) {
			const Array<T>& track = tracks.tracks[0];
			for (size_t i = 0; i < nSamplesPerTrack; ++i) {
				// Convert to 32-bit integer, then shift
				int32 sample;
				convertDataTo(track[i], sample);
				int32 newSample = (sample>>8);
				// Round up
				newSample += ((sample>>7) & 1) && (newSample != 0x7FFFFF);
				data[0] = uint8(newSample);
				data[1] = uint8(newSample>>8);
				data[2] = uint8(newSample>>16);
				data += 3;
			}
		}
		else {
			for (size_t blocki = 0; blocki < nSamplesPerTrack; ++blocki) {
				for (size_t tracki = 0; tracki < nTracks; ++tracki) {
					const Array<T>& track = tracks.tracks[tracki];
					if (blocki < track.size()) {
						// Convert to 32-bit integer, then shift
						int32 sample;
						convertDataTo(track[blocki], sample);
						int32 newSample = (sample>>8);
						// Round up
						newSample += ((sample>>7) & 1) && (newSample != 0x7FFFFF);
						data[0] = uint8(newSample);
						data[1] = uint8(newSample>>8);
						data[2] = uint8(newSample>>16);
					}
					else {
						data[0] = 0;
						data[1] = 0;
						data[2] = 0;
					}
					data += 3;
				}
			}
		}
	}
	else if (outputBytesPerSample == 4) {
		int32* data = reinterpret_cast<int32*>(contents);
		interleaveTrackData(tracks.tracks, nSamplesPerTrack, data);
	}

	if (sampleDataSize & 1) {
		// For completeness, initialize the padding byte to 0.
		contents[sampleDataSize] = 0;
	}

	bool success = WriteWholeFile(filename, contentStart, size);
	return success;
}

template bool WriteWAVFile<uint8>(const char* filename, const AudioTracks<uint8>& tracks, size_t outputBitsPerSample);
template bool WriteWAVFile<int16>(const char* filename, const AudioTracks<int16>& tracks, size_t outputBitsPerSample);
template bool WriteWAVFile<float>(const char* filename, const AudioTracks<float>& tracks, size_t outputBitsPerSample);
template bool WriteWAVFile<double>(const char* filename, const AudioTracks<double>& tracks, size_t outputBitsPerSample);

} // namespace wav
OUTER_NAMESPACE_END
