#pragma once

// This file contains declarations of functions for reading and writing
// music data from MIDI (Musical Instrument Digital Interface) format files
// into a simpler in-memory format.
// Definitions of the functions are in MIDI.cpp

#include "Types.h"
#include "Array.h"

OUTER_NAMESPACE_BEGIN
namespace midi {

using namespace COMMON_LIBRARY_NAMESPACE;

struct Note {
	// Start time of the note in MIDI ticks
	size_t startTime;

	// Duration of the note in MIDI ticks
	size_t duration;

	// Pitch number of the note, in range 0-127
	uint8 pitch;

	// "Velocity" of the note, (similar to volume), in range 0-127
	uint8 velocity;
};

enum class FullEventType {
	SEQENCE_NUMBER = 0x00,  // 1 integer
	TEXT,                   // Text
	COPYRIGHT_NOTICE,       // Text
	TRACK_NAME,             // Text
	INSTRUMENT_NAME,        // Text
	LYRIC,                  // Text
	MARKER,                 // Text
	CUE_POINT,              // Text

	// The following 5 have an associated channel
	POLYPHONIC_AFTERTOUCH = 10,
	CONTROL_MODE_CHANGE,
	PROGRAM_CHANGE,
	CHANNEL_AFTERTOUCH,
	PITCH_WHEEL_RANGE,

	// 1 integer for channel number
	CHANNEL_PREFIX = 0x20,

	// 1 integer in microseconds per quarter note (up to 2^24 - 1)
	SET_TEMPO = 0x51,

	// 1 integer for seconds, 1 integer frame, 1 integer hundredth of a frame
	SMPTE_OFFSET = 0x54,

	// 1 integer split into numerator and denominator (0=whole,1=half,2=quarter,3=eighth...)
	// 1 integer # of MIDI clocks per metronome click
	// 1 integer # of notated 32ndth notes per 24 MIDI clocks (1 quarter note)
	TIME_SIGNATURE = 0x58,

	// 1 integer # of flats (-ve) or sharps (+ve)
	// 1 integer 0=major 1=minor
	KEY_SIGNATURE = 0x59,
};

struct Event {
	size_t time;
	FullEventType type;
	int64 numbers[3];

	Array<char> text;
};

struct TrackChannel {
	size_t trackNum;

	// Channel number, in range 0-15, though most programs name them 1-16.
	// Channel 9 (usually named 10) is reserved for percussion.
	size_t channelNum;

	// Since note on and note off are by far the most common and most useful
	// events, this separate array of note data is here for convenience.
	Array<Note> notes;

	Array<Event> events;
};

struct MIDITracks {
	size_t ticksPerUnit;
	Array<TrackChannel> trackChannels;
};

COMMON_LIBRARY_EXPORTED bool ReadMIDIFile(const char* filename, MIDITracks& tracks);
COMMON_LIBRARY_EXPORTED bool WriteMIDIFile(const char* filename, const MIDITracks& tracks);

} // namespace midi
OUTER_NAMESPACE_END
