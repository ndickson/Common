// This file contains definitions of functions for reading and writing
// music data from MIDI (Musical Instrument Digital Interface) format files
// into a simpler in-memory format.
// Declarations of the functions are in MIDI.h

#include "midi/MIDI.h"
#include "midi/MIDIFormat.h"
#include "File.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"

#define MIDI_DEBUG 0

OUTER_NAMESPACE_BEGIN
namespace midi {

bool ReadMIDIFile(const char* filename, MIDITracks& tracks) {
	// Pre-emptively clear tracks, in case there's no valid content.
	tracks.ticksPerUnit = 0;
	tracks.trackChannels.setSize(0);

	Array<char> contents;
	bool success = ReadWholeFile(filename, contents);
	if (!success || contents.size() < sizeof(FileHeader)) {
		return false;
	}

	const char* data = contents.data();
	size_t size = contents.size();
	const FileHeader* header = reinterpret_cast<const FileHeader*>(data);
	if (!header->isSignatureCorrect() || !header->isSizeCorrect()) {
		return false;
	}
	uint16 format = header->getFormat();
	if (format > 2) {
		return false;
	}

	bool isSMPTE = header->isSMPTE();
	uint16 ticksPerUnit = header->getTicksPerUnit();
	uint8 SMPTEFormat = isSMPTE ? header->getSMTPEFormat() : 0;
	tracks.ticksPerUnit = ticksPerUnit;

	uint16 nTracks = header->getNumTracks();
	if (format == 0 && nTracks != 1) {
		return false;
	}

	data += sizeof(FileHeader);
	size -= sizeof(FileHeader);

	Array<Event> tempoTrack;

	for (int trackNum = 0; trackNum < nTracks; ++trackNum) {
		if (size < sizeof(TrackHeader)) {
			return false;
		}

		const TrackHeader* trackHeader = reinterpret_cast<const TrackHeader*>(data);
		if (!trackHeader->isSignatureCorrect()) {
			return false;
		}

		data += sizeof(TrackHeader);
		size -= sizeof(TrackHeader);

		size_t trackSize = trackHeader->getSize();
		if (size < trackSize) {
			return false;
		}

		const char* endOfTrack = data + trackSize;
		size -= trackSize;

		size_t currentTime = 0;
		int currentChannel = -1;
		size_t currentOutputTrack;

		struct TrackAndIndex {
			size_t track;
			size_t index;
		};

		BufArray<TrackAndIndex,16> activeNotes;

		// At least two bytes are needed for an event, and there may or may not be an extra padding byte added at the end (most likely not)
		while (trackSize >= 2) {
			const char* intData = data;
			int deltaTime = parseVariableSizeInt(intData, data+trackSize);
			if (deltaTime < 0 || intData == data+trackSize) {
				return false;
			}
			currentTime += deltaTime;
			size_t intSize = (intData-data);
			trackSize -= intSize;
			data = intData;

			const EventCode& eventCode = *reinterpret_cast<const EventCode*>(data);
			EventType eventType = EventType(eventCode.type);
			if (eventType == EventType::NOTE_OFF || eventType == EventType::NOTE_ON) {
				// All notes are 3 bytes long.
				if (trackSize < 3) {
					return false;
				}
				const size_t channel = eventCode.channel;
				const char notePitch = data[1];
				const char noteVelocity = data[2];
				data += 3;
				trackSize -= 3;
				if ((notePitch&0x80) || (noteVelocity&0x80)) {
					return false;
				}
				const bool isOn = eventType == EventType::NOTE_ON && noteVelocity > 0;
				if (isOn) {
					if (channel != currentChannel) {
						// Find the output track with this input track and channel.
						size_t numOutputTracks = tracks.trackChannels.size();
						for (size_t outputTracki = 0; outputTracki != numOutputTracks; ++outputTracki) {
							if (tracks.trackChannels[outputTracki].trackNum == trackNum &&
								tracks.trackChannels[outputTracki].channelNum == channel
							) {
								currentOutputTrack = outputTracki;
								currentChannel = int(channel);
								break;
							}
						}

						// If none exists yet, create one.
						if (channel != currentChannel) {
							currentOutputTrack = numOutputTracks;
							tracks.trackChannels.setSize(numOutputTracks + 1);
							TrackChannel& outputTrack = tracks.trackChannels.last();
							outputTrack.trackNum = trackNum;
							outputTrack.channelNum = channel;
							currentChannel = int(channel);
						}
					}
					TrackChannel& outputTrack = tracks.trackChannels[currentOutputTrack];
					activeNotes.append(TrackAndIndex{currentOutputTrack, outputTrack.notes.size()});

					// Make the notes duration zero until they're ended.
					// If they don't get ended, they'll be identifiable by having duration zero.
					outputTrack.notes.append(Note{currentTime, 0, uint8(notePitch), uint8(noteVelocity)});
				}
				else {
					// Find the corresponding active note
					bool endedNote = false;
					for (size_t i = 0, n = activeNotes.size(); i < n; ++i) {
						const TrackAndIndex& trackAndIndex = activeNotes[i];
						TrackChannel& otherTrack = tracks.trackChannels[trackAndIndex.track];
						Note& otherNote = otherTrack.notes[trackAndIndex.index];
						if (otherTrack.channelNum == channel && otherNote.pitch == notePitch) {
							otherNote.duration = currentTime - otherNote.startTime;

							// Remove the note from activeNotes.
							// NOTE: This works even if i == n-1
							activeNotes[i] = activeNotes.last();
							activeNotes.setSize(n-1);
							endedNote = true;
							break;
						}
					}
					if (!endedNote) {
						return false;
					}
				}
			}
			else if (eventType == EventType::POLYPHONIC_AFTERTOUCH || eventType == EventType::CONTROL_MODE_CHANGE || eventType == EventType::PITCH_WHEEL_RANGE) {
				// These types of events are 3 bytes long.
				if (trackSize < 3) {
					return false;
				}
				// These events are currently ignored.
				//const size_t channel = eventCode.channel;
				data += 3;
				trackSize -= 3;
			}
			else if (eventType == EventType::PROGRAM_CHANGE || eventType == EventType::CHANNEL_AFTERTOUCH) {
				// These types of events are 2 bytes long.
				if (trackSize < 2) {
					return false;
				}
				// These events are currently ignored.
				//const size_t channel = eventCode.channel;
				data += 2;
				trackSize -= 2;
			}
			else if (eventType == EventType::SYSTEM_EXCLUSIVE) {
				++data;
				--trackSize;
				if (SysExEventCode(eventCode.code) == SysExEventCode::START || SysExEventCode(eventCode.code) == SysExEventCode::CONTINUED) {
					// Variable size integer requires at least 1 byte.
					if (trackSize < 1) {
						return false;
					}
					const char* intData = data;
					int messageLength = parseVariableSizeInt(intData, data+trackSize);
					trackSize -= (intData - data);
					data = intData;
					if (messageLength < 0 || messageLength > trackSize) {
						return false;
					}

					// Currently ignored

					data += messageLength;
					trackSize -= messageLength;
				}
				else if (SysExEventCode(eventCode.code) == SysExEventCode::META) {
					// Need at least 2 bytes for MetaEventType and variable size integer.
					if (trackSize < 2) {
						return false;
					}
					const MetaEventType metaEvent = MetaEventType(*data);
					++data;
					--trackSize;
					const char* intData = data;
					int messageLength = parseVariableSizeInt(intData, data+trackSize);
					// FIXME: Is the int length included in messageLength in this case?
					trackSize -= (intData - data);
					data = intData;
					if (messageLength < 0 || messageLength > trackSize) {
						return false;
					}

					if (metaEvent == MetaEventType::SET_TEMPO) {
						if (messageLength == 3) {
							uint32 microsecondsPerQuarterNote =
								(uint32(uint8(data[0]))<<16) | (uint32(uint8(data[1]))<<8) | uint32(uint8(data[2]));
							Event event;
							event.time = currentTime;
							event.type = FullEventType::SET_TEMPO;
							event.numbers[0] = microsecondsPerQuarterNote;
							event.numbers[1] = 0;
							event.numbers[2] = 0;

#if MIDI_DEBUG
							printf("Tempo of quarter = %f (%u us/quarter) at time %zu\n", (60.0*1e6)/microsecondsPerQuarterNote, microsecondsPerQuarterNote, currentTime);
							fflush(stdout);
#endif

							tempoTrack.append(std::move(event));
						}
					}
					else if (metaEvent == MetaEventType::TIME_SIGNATURE) {
						if (messageLength == 4) {
							uint32 numerator = data[0];
							uint32 logDenominator = data[1];
							Event event;
							event.time = currentTime;
							event.type = FullEventType::TIME_SIGNATURE;
							event.numbers[0] = (int64(numerator)<<32) | int64(logDenominator);
							event.numbers[1] = int64(data[2]);
							event.numbers[2] = int64(data[3]);

#if MIDI_DEBUG
							printf("Time signature of %u/%u (additional data: %u %u) at time %zu\n", numerator, uint32(1)<<logDenominator, uint32(data[2]), uint32(data[3]), currentTime);
							fflush(stdout);
#endif

							tempoTrack.append(std::move(event));
						}
					}
					else if (metaEvent == MetaEventType::KEY_SIGNATURE) {
						if (messageLength == 2) {
							Event event;
							event.time = currentTime;
							event.type = FullEventType::KEY_SIGNATURE;
							event.numbers[0] = int64(int8(data[0]));
							event.numbers[1] = int64(data[1]);
							event.numbers[2] = 0;

#if MIDI_DEBUG
							printf("Key signature of %d sharps %s at time %zu\n", int(event.numbers[0]), event.numbers[1] ? "major" : "minor", currentTime);
							fflush(stdout);
#endif

							tempoTrack.append(std::move(event));
						}
					}

					// Currently ignored

					data += messageLength;
					trackSize -= messageLength;
				}
				else {
					// Unsupported SysExEventCode
					// The size is unclear, so nothing following it can be handled.
					return false;
				}

			}
			else {
				// Invalid event type
				return false;
			}
		}

		// This may skip an extra padding byte at the end of the track.
		data = endOfTrack;
	}

	if (tempoTrack.size() != 0) {
		tracks.trackChannels.setSize(tracks.trackChannels.size() + 1);
		TrackChannel& outputTrack = tracks.trackChannels.last();
		outputTrack.trackNum = nTracks;
		outputTrack.channelNum = 0;
		outputTrack.events = std::move(tempoTrack);
	}

	return true;
}

bool WriteMIDIFile(const char* filename, const MIDITracks& tracks)
{
	// FIXME: Implement this!!!
	return false;
}

} // namespace midi
OUTER_NAMESPACE_END
