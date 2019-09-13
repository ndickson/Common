// This file contains definitions of functions for reading/writing files.

#include "File.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"
#include "text/UTF.h"

#ifdef _WIN32
#include <Windows.h>
#include <string.h>
#else
#include <stdio.h>
#endif

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

bool ReadWholeFile(const char* filename, Array<char>& contents) {
#if _WIN32
	// Windows: use Windows API, to avoid UTF problems
	// with C++ standard library functions on Windows.
	size_t utf8Length = strlen(filename);
	size_t utf16Length = text::UTF16Length(filename, utf8Length);
	static_assert(sizeof(wchar_t) == sizeof(uint16));
	BufArray<uint16, MAX_PATH> filenameUTF16;
	filenameUTF16.setSize(utf16Length+1);
	text::UTF8ToUTF16(filename, utf8Length, filenameUTF16.data());
	filenameUTF16[utf16Length] = 0;
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
	HANDLE fileHandle = CreateFile2((LPCWSTR)filenameUTF16.data(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
#else
	HANDLE fileHandle = CreateFileW((LPCWSTR)filenameUTF16.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, INVALID_HANDLE_VALUE);
#endif
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return false;
	}

#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	// GetFileSizeEx isn't available in UWP, so use GetFileInformationByHandleEx.
	FILE_STANDARD_INFO fileInfo;
	bool success = (GetFileInformationByHandleEx(fileHandle, FileStandardInfo, &fileInfo, sizeof(fileInfo)) != 0);
#else
	LARGE_INTEGER fileSizeStruct;
	bool success = (GetFileSizeEx(fileHandle, &fileSizeStruct) != 0);
#endif

	if (success) {
#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
		uint64 fileSize = fileInfo.EndOfFile.QuadPart;
#else
		uint64 fileSize = fileSizeStruct.QuadPart;
#endif
		contents.setSize(fileSize);

		if (fileSize < (1ULL<<31)) {
			// Single read
			// A threshold of (1ULL<<32) above would probably work, too,
			// but it's 31 just to be safe.
			DWORD numBytesRead;
			success = ReadFile(fileHandle, contents.data(), (DWORD)fileSize, &numBytesRead, nullptr) != 0;
			// NOTE: Unlike using &=, this avoids reading numBytesRead when success is already false,
			// which may avoid a runtime check failure in debug builds.
			success = (success && (fileSize == numBytesRead));
		}
		else {
			// Do multiple reads
			char* data = contents.data();
			uint64 remaining = fileSize;
			do {
				uint32 partialReadSize = (1<<30);
				if (remaining < partialReadSize) {
					partialReadSize = uint32(remaining);
				}
				DWORD numBytesRead;
				success = ReadFile(fileHandle, data, partialReadSize, &numBytesRead, nullptr) != 0;
				success = (success && (partialReadSize == numBytesRead));
				remaining -= partialReadSize;
				data += partialReadSize;
			} while (success && remaining != 0);
		}

		if (!success) {
			contents.setCapacity(0);
		}
	}

	CloseHandle(fileHandle);

	return success;
#else
	// Non-Windows platforms
	FILE* file = fopen(filename, "rb");
	if (file == nullptr) {
		return false;
	}

	bool success = (fseek(file, 0, SEEK_END) == 0);
	if (success) {
		auto fileSize = ftell(file);
		success = (fileSize >= 0);
		if (success) {
			success = (fseek(file, 0, SEEK_SET) == 0);
			if (success) {
				contents.setSize(fileSize);
				success = (contents.data() != nullptr);
				if (success) {
					size_t numBytesRead = fread(contents.data(), 1, fileSize, file);
					success = (fileSize == numBytesRead);
					if (!success) {
						contents.setCapacity(0);
					}
				}
			}
		}
	}

	fclose(file);

	return success;
#endif
}

bool WriteWholeFile(const char* filename, const char* contents, size_t length) {
#if _WIN32
	// Windows: use Windows API, to avoid UTF problems
	// with C++ standard library functions on Windows.
	size_t utf8Length = strlen(filename);
	size_t utf16Length = text::UTF16Length(filename, utf8Length);
	static_assert(sizeof(wchar_t) == sizeof(uint16));
	BufArray<uint16, MAX_PATH> filenameUTF16;
	filenameUTF16.setSize(utf16Length+1);
	text::UTF8ToUTF16(filename, utf8Length, filenameUTF16.data());
	filenameUTF16[utf16Length] = 0;
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
	HANDLE fileHandle = CreateFile2((LPCWSTR)filenameUTF16.data(), GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
#else
	HANDLE fileHandle = CreateFileW((LPCWSTR)filenameUTF16.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, INVALID_HANDLE_VALUE);
#endif
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return false;
	}

	bool success;
	if (length < (1ULL<<31)) {
		// Single write
		// A threshold of (1ULL<<32) above would probably work, too,
		// but it's 31 just to be safe.
		DWORD numBytesWritten;
		success = WriteFile(fileHandle, contents, (DWORD)length, &numBytesWritten, nullptr) != 0;
		// NOTE: Unlike using &=, this avoids reading numBytesWritten when success is already false,
		// which may avoid a runtime check failure in debug builds.
		success = (success && (length == numBytesWritten));
	}
	else {
		// Do multiple writes
		uint64 remaining = length;
		do {
			uint32 partialWriteSize = (1<<30);
			if (remaining < partialWriteSize) {
				partialWriteSize = uint32(remaining);
			}
			DWORD numBytesWritten;
			success = WriteFile(fileHandle, contents, partialWriteSize, &numBytesWritten, nullptr) != 0;
			success = (success && (partialWriteSize == numBytesWritten));
			remaining -= partialWriteSize;
			contents += partialWriteSize;
		} while (success && remaining != 0);
	}

	CloseHandle(fileHandle);

	return success;
#else
	// Non-Windows platforms
	FILE* file = fopen(filename, "wb");
	if (file == nullptr) {
		return false;
	}

	size_t numBytesWritten = fwrite(contents, 1, length, file);
	bool success = (length == numBytesWritten);

	fclose(file);

	return success;
#endif
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
