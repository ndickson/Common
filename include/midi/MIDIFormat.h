#pragma once

// This file contains structures, enumerations, and functions directly
// related to MIDI (Musical Instrument Digital Interface) file format.

#include "Types.h"

OUTER_NAMESPACE_BEGIN
namespace midi {

using namespace Common;

// Enable struct packing, to avoid any padding bytes, since
// MIDI file format structures can otherwise end up with extra bytes.
#pragma pack(push, 1)

struct alignas(1) FileHeader {
	uint32  signature;  // "MThd"
	uint8   size[4];    // big-endian value 6
	uint8   format[2];  // big-endian value 0, 1, or 2
	uint8   nTracks[2]; // big-endian number of tracks (always 1 for format 0)
	uint8   division[2];// big-endian

	bool isSizeCorrect() const {
		static_assert(sizeof(FileHeader) == 14, "FileHeader can't have any padding bytes in it.");

		return (size[0]==0) && (size[1]==0) && (size[2]==0) && (size[3]==6);
	}

	bool isSignatureCorrect() const {
		return signature == (uint32('M') | (uint32('T')<<8) | (uint32('h')<<16) | (uint32('d')<<24));
	}

	uint16 getFormat() const {
		return (uint16(format[0])<<8) | uint16(format[1]);
	}

	uint16 getNumTracks() const {
		return (uint16(nTracks[0])<<8) | uint16(nTracks[1]);
	}

	bool isSMPTE() const {
		return (division[0]>>7)&1;
	}

	uint16 getTicksPerUnit() const {
		if (isSMPTE()) {
			return uint16(division[1]);
		}
		return (uint16(division[0])<<8) | uint16(division[1]);
	}

	uint8 getSMTPEFormat() const {
		return uint8(division[0] & ~0x80);
	}
};

struct alignas(1) TrackHeader {
	uint32  signature;  // "MTrk"
	uint8   size[4];    // big-endian value length of the rest of the data in bytes

	bool isSignatureCorrect() const {
		static_assert(sizeof(TrackHeader) == 8, "TrackHeader can't have any padding bytes in it.");

		return signature == (uint32('M') | (uint32('T')<<8) | (uint32('r')<<16) | (uint32('k')<<24));
	}

	uint32 getSize() const {
		return (uint32(size[0])<<24) | (uint32(size[1])<<16) | (uint32(size[2])<<8) | uint32(size[3]);
	}
};

// MIDI uses positive integers encoded with a variable number of bytes in several situations.
// Each byte contains 7 bits of data, with higher bits coming in earlier bytes.
// The high bit of each byte is 1 if there is another byte in the integer encoding,
// and is 0 if there are no more bytes in the integer encoding.
// These integers are only allowed to be up to 4 bytes long.
// This function returns the integer, or -1 if the encoding was invalid.
// It also modifies the data pointer passed in to point to the end of the integer.
constexpr inline int parseVariableSizeInt(const char*& data, const char* end) {
	if (data == end) {
		// Already at end of data, so return error
		return -1;
	}

	constexpr char continueBitMask = char(0x80);

	char c = *data;
	++data;
	if (!(c & continueBitMask)) {
		// Common case of a single byte, for values 0-127
		return int(c);
	}

	// Maximum of 4 bytes allowed
	if (end > data+4) {
		end = data+4;
	}

	int value = int(c & ~continueBitMask);

	for (; data != end; ++data) {
		// Higher bits come earlier in the data bytes of this integer (big-endian-ish).
		// 7 data bits per byte in this encoding
		value <<= 7;
		c = *data;
		++data;
		value |= int(c & ~continueBitMask);
		if (!(c & continueBitMask)) {
			// Finished integer, so return
			return value;
		}
	}

	// Reached end of data before end of encoded integer, so return error
	return -1;
}

// Values for EventCode::type
// 0-7 appear to be undefined
enum class EventType {
	NOTE_OFF = 8,
	NOTE_ON,
	POLYPHONIC_AFTERTOUCH,
	CONTROL_MODE_CHANGE,
	PROGRAM_CHANGE,
	CHANNEL_AFTERTOUCH,
	PITCH_WHEEL_RANGE,
	SYSTEM_EXCLUSIVE
};

// If EventCode::type is SYSTEM_EXCLUSIVE (0xF), these are code values (including type).
enum class SysExEventCode {
	START = 0xF0,
	CONTINUED = 0xF7,
	META = 0xFF
};

// This immediately follows the variable-length integer for the change in time between the previous event and this one.
union EventCode {
	uint8 code;
	struct {
		uint8 channel:4;    // Channel number (0-15, usually named 1-16)
		uint8 type:4;       // EventType
	};
};

// If EventCode::code is SYSEX_META, this value is the byte that follows.
enum class MetaEventType {
	SEQENCE_NUMBER = 0x00,  // Length should be 2; 2-byte integer
	TEXT,                   // Text
	COPYRIGHT_NOTICE,       // Text
	TRACK_NAME,             // Text
	INSTRUMENT_NAME,        // Text
	LYRIC,                  // Text
	MARKER,                 // Text
	CUE_POINT,              // Text
	CHANNEL_PREFIX = 0x20,  // Length should be 1; 1 byte for channel number
	END_OF_TRACK = 0x2F,    // Length should be 0
	SET_TEMPO = 0x51,       // Length should be 3; 3-byte integer in microseconds per quarter note
	SMPTE_OFFSET = 0x54,    // Length should be 5; 1-byte hour, 1-byte minute, 1-byte second, 1-byte frame, 1-byte hundredth of a frame
	TIME_SIGNATURE = 0x58,  // Length should be 4; 1-byte numerator, 1-byte denominator (0=whole,1=half,2=quarter,3=eighth...), 1-byte # of MIDI clocks per metronome click, 1-byte # of notated 32ndth notes per 24 MIDI clocks (1 quarter note)
	KEY_SIGNATURE = 0x59,   // Length should be 2; 1-byte # of flats (-ve) or sharps (+ve), 1-byte 0=major 1=minor
	SEQUENCER_SPECIFIC = 0x7F
};

#pragma pack(pop)

} // namespace midi
OUTER_NAMESPACE_END
