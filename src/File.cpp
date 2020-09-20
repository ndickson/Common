// This file contains definitions of functions for reading/writing files.
#include "File.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"
#include "text/UTF.h"
#include "text/TextFunctions.h"

#include <atomic>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
// Remove problematic defines that conflict with proper function or enum member names.
#undef CreateFile
#undef ABSOLUTE
#undef RELATIVE
#else
#include <stdio.h>
#endif

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

#if _WIN32
static HANDLE CreateFileWrapper(const char* filename, DWORD accessMode, DWORD sharedMode, DWORD creationMode) {
	// Windows: use Windows API, to avoid UTF problems
	// with C++ standard library functions on Windows.
	size_t utf8Length = text::stringSize(filename);
	size_t utf16Length = text::UTF16Length(filename, utf8Length);
	static_assert(sizeof(wchar_t) == sizeof(uint16));
	BufArray<uint16, MAX_PATH> filenameUTF16;
	filenameUTF16.setSize(utf16Length+1);
	text::UTF8ToUTF16(filename, utf8Length, filenameUTF16.data());
	filenameUTF16[utf16Length] = 0;
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
	HANDLE fileHandle = CreateFile2((LPCWSTR)filenameUTF16.data(), accessMode, sharedMode, creationMode, nullptr);
#else
	HANDLE fileHandle = CreateFileW((LPCWSTR)filenameUTF16.data(), accessMode, sharedMode, nullptr, creationMode, 0, INVALID_HANDLE_VALUE);
#endif
	return fileHandle;
}
#endif

// Returns true on success (even if no directories were created), and false on failure.
COMMON_LIBRARY_EXPORTED bool CreateMissingDirectories(const char* filename) {
	size_t currentBegin = 0;
	for (size_t texti = 0; filename[texti] != 0; ++texti) {
		char c = filename[texti];
		if (c == '/' || c == '\\') {
			if (currentBegin == texti) {
				// Skip duplicate or initial slashes
				++currentBegin;
			}
			// Check if directory already exists
			// TODO: Find a way that avoids copying the string; std::filesystem::path constructs a std::wstring.
			const std::filesystem::path currentPath(filename, filename+texti);
			const std::filesystem::file_status status = std::filesystem::status(currentPath);
			if (status.type() == std::filesystem::file_type::not_found) {
				// Create new directory.
				bool success = std::filesystem::create_directory(currentPath);
				if (!success) {
					// Possibly an invalid filename
					return false;
				}
			}
			else if (status.type() != std::filesystem::file_type::directory) {
				// Conflicting file that's not a directory!
				return false;
			}
			// Next name starts after the slash.
			currentBegin = texti+1;
		}
	}

	return true;
}


COMMON_LIBRARY_EXPORTED bool ReadWholeFile(const char* filename, Array<char>& contents) {
#if _WIN32
	HANDLE fileHandle = CreateFileWrapper(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
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
			success = ::ReadFile(fileHandle, contents.data(), (DWORD)fileSize, &numBytesRead, nullptr) != 0;
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
				success = ::ReadFile(fileHandle, data, partialReadSize, &numBytesRead, nullptr) != 0;
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
		// FIXME: This may or may not fail correctly for large files on systems
		// where ftell returns a 32-bit integer.
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

COMMON_LIBRARY_EXPORTED bool WriteWholeFile(const char* filename, const void* contents, size_t length) {
	bool success = CreateMissingDirectories(filename);
	if (!success) {
		return false;
	}
#if _WIN32
	HANDLE fileHandle = CreateFileWrapper(filename, GENERIC_WRITE, 0, CREATE_ALWAYS);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (length < (1ULL<<31)) {
		// Single write
		// A threshold of (1ULL<<32) above would probably work, too,
		// but it's 31 just to be safe.
		DWORD numBytesWritten;
		success = ::WriteFile(fileHandle, contents, (DWORD)length, &numBytesWritten, nullptr) != 0;
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
			success = ::WriteFile(fileHandle, contents, partialWriteSize, &numBytesWritten, nullptr) != 0;
			success = (success && (partialWriteSize == numBytesWritten));
			remaining -= partialWriteSize;
			((const char*&)contents) += partialWriteSize;
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
	success = (length == numBytesWritten);

	fclose(file);

	return success;
#endif
}


class FileHandle::InternalHandle {
public:
	std::atomic<size_t> referenceCount;
#if _WIN32
	HANDLE fileHandle;
#else
	FILE* file;
#endif

#if _WIN32
	InternalHandle(HANDLE fileHandle_) : referenceCount(1), fileHandle(fileHandle_) {
		assert(fileHandle != INVALID_HANDLE_VALUE);
	}
#else
	InternalHandle(FILE* file_) : referenceCount(1), file(file_) {
		assert(file != nullptr);
}
#endif

	~InternalHandle() {
		assert(referenceCount.load(std::memory_order_relaxed) == 0);
#if _WIN32
		CloseHandle(fileHandle);
#else
		fclose(file);
#endif
	}
};

COMMON_LIBRARY_EXPORTED void FileHandle::incrementRefCount() noexcept {
	assert(p != nullptr);
	++(p->referenceCount);
	assert(p->referenceCount.load(std::memory_order_relaxed) != 0);
}

COMMON_LIBRARY_EXPORTED void FileHandle::decrementRefCount() noexcept {
	assert(p != nullptr);
	auto count = --(p->referenceCount);
	if (count == 0) {
		delete p;
	}
}

COMMON_LIBRARY_EXPORTED size_t FileHandle::getRefCount() noexcept {
	if (p == nullptr) {
		return 0;
	}
	return p->referenceCount.load(std::memory_order_relaxed);
}

COMMON_LIBRARY_EXPORTED WriteFileHandle CreateFile(const char* filename) {
	bool success = CreateMissingDirectories(filename);
	if (!success) {
		return WriteFileHandle();
	}
#ifdef _WIN32
	HANDLE fileHandle = CreateFileWrapper(filename, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, CREATE_ALWAYS);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return WriteFileHandle();
	}
	return WriteFileHandle(new FileHandle::InternalHandle(fileHandle));
#else
	FILE* file = fopen(filename, "wb");
	if (file == nullptr) {
		return WriteFileHandle();
	}
	return WriteFileHandle(new FileHandle::InternalHandle(file));
#endif
}

COMMON_LIBRARY_EXPORTED ReadFileHandle OpenFileRead(const char* filename) {
#ifdef _WIN32
	HANDLE fileHandle = CreateFileWrapper(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return ReadFileHandle();
	}
	return ReadFileHandle(new FileHandle::InternalHandle(fileHandle));
#else
	FILE* file = fopen(filename, "rb");
	if (file == nullptr) {
		return ReadFileHandle();
	}
	return ReadFileHandle(new FileHandle::InternalHandle(file));
#endif
}

COMMON_LIBRARY_EXPORTED WriteFileHandle OpenFileAppend(const char* filename) {
	bool success = CreateMissingDirectories(filename);
	if (!success) {
		return WriteFileHandle();
	}
#ifdef _WIN32
	HANDLE fileHandle = CreateFileWrapper(filename, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_ALWAYS);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return WriteFileHandle();
	}
	// Start at the end, in order to append.
	LARGE_INTEGER position;
	position.QuadPart = 0;
	BOOL success2 = SetFilePointerEx(fileHandle, position, nullptr, FILE_END);
	if (!success2) {
		CloseHandle(fileHandle);
		return WriteFileHandle();
	}
	return WriteFileHandle(new FileHandle::InternalHandle(fileHandle));
#else
	// The + is just so that we can get the file size using fseek and ftell.
	FILE* file = fopen(filename, "a+b");
	if (file == nullptr) {
		return WriteFileHandle();
	}
	return WriteFileHandle(new FileHandle::InternalHandle(file));
#endif
}

COMMON_LIBRARY_EXPORTED ReadWriteFileHandle OpenFileReadWrite(const char* filename) {
	bool success = CreateMissingDirectories(filename);
	if (!success) {
		return ReadWriteFileHandle();
	}
#ifdef _WIN32
	HANDLE fileHandle = CreateFileWrapper(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_ALWAYS);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return ReadWriteFileHandle();
	}
	return ReadWriteFileHandle(new FileHandle::InternalHandle(fileHandle));
#else
	// First, try opening an existing file for read and write.
	FILE* file = fopen(filename, "r+b");
	if (file == nullptr) {
		// File might not exist; try creating it for read and write.
		file = fopen(filename, "w+b");
		if (file == nullptr) {
			return ReadWriteFileHandle();
		}
	}
	return ReadWriteFileHandle(new FileHandle::InternalHandle(file));
#endif
}

#ifdef _WIN32
COMMON_LIBRARY_EXPORTED size_t ReadFileInternal(HANDLE fileHandle, void* dataFromFile, size_t lengthToRead) {
	assert(fileHandle != INVALID_HANDLE_VALUE);
	if (lengthToRead < (1ULL<<31)) {
		// Single read
		// A threshold of (1ULL<<32) above would probably work, too,
		// but it's 31 just to be safe.
		// Initialize numBytesRead to zero, since ReadFile apparently might not write to it
		// if immediate failure occurs, and we don't want uninitialized values returned.
		DWORD numBytesRead = 0;
		::ReadFile(fileHandle, dataFromFile, (DWORD)lengthToRead, &numBytesRead, nullptr);
		return size_t(numBytesRead);
	}

	// Do multiple reads
	char* data = (char*)dataFromFile;
	uint64 remaining = lengthToRead;
	bool success;
	do {
		uint32 partialReadSize = (1<<30);
		if (remaining < partialReadSize) {
			partialReadSize = uint32(remaining);
		}
		DWORD numBytesRead = 0;
		success = ::ReadFile(fileHandle, data, partialReadSize, &numBytesRead, nullptr) != 0;
		success = (success && (partialReadSize == numBytesRead));
		remaining -= numBytesRead;
		data += partialReadSize;
	} while (success && remaining != 0);
	return lengthToRead - size_t(remaining);
}
#else
COMMON_LIBRARY_EXPORTED size_t ReadFileInternal(FILE* filePointer, void* dataFromFile, size_t lengthToRead) {
	assert(filePointer != nullptr);
	return fread(dataFromFile, 1, lengthToRead, filePointer);
}
#endif

COMMON_LIBRARY_EXPORTED size_t ReadFile(const ReadFileHandle& file, void* dataFromFile, size_t lengthToRead) {
	if (file.p == nullptr || lengthToRead == 0 || dataFromFile == nullptr) {
		return 0;
	}
#ifdef _WIN32
	return ReadFileInternal(file.p->fileHandle, dataFromFile, lengthToRead);
#else
	return ReadFileInternal(file.p->file, dataFromFile, lengthToRead);
#endif
}

COMMON_LIBRARY_EXPORTED size_t ReadFile(const ReadWriteFileHandle& file, void* dataFromFile, size_t lengthToRead) {
	if (file.p == nullptr || lengthToRead == 0 || dataFromFile == nullptr) {
		return 0;
	}
#ifdef _WIN32
	return ReadFileInternal(file.p->fileHandle, dataFromFile, lengthToRead);
#else
	return ReadFileInternal(file.p->file, dataFromFile, lengthToRead);
#endif
}

COMMON_LIBRARY_EXPORTED size_t WriteFile(const WriteFileHandle& file, const void* dataToWrite, size_t lengthToWrite) {
	if (file.p == nullptr || lengthToWrite == 0 || dataToWrite == nullptr) {
		return 0;
	}
#ifdef _WIN32
	HANDLE fileHandle = file.p->fileHandle;
	assert(fileHandle != INVALID_HANDLE_VALUE);
	if (lengthToWrite < (1ULL<<31)) {
		// Single write
		// A threshold of (1ULL<<32) above would probably work, too,
		// but it's 31 just to be safe.
		// Initialize numBytesWritten to zero, since WriteFile apparently might not write to it
		// if immediate failure occurs, and we don't want uninitialized values returned.
		DWORD numBytesWritten = 0;
		::WriteFile(fileHandle, dataToWrite, (DWORD)lengthToWrite, &numBytesWritten, nullptr);
		return size_t(numBytesWritten);
	}

	// Do multiple reads
	const char* data = (const char*)dataToWrite;
	uint64 remaining = lengthToWrite;
	bool success;
	do {
		uint32 partialWriteSize = (1<<30);
		if (remaining < partialWriteSize) {
			partialWriteSize = uint32(remaining);
		}
		DWORD numBytesWritten = 0;
		success = ::WriteFile(fileHandle, data, partialWriteSize, &numBytesWritten, nullptr) != 0;
		success = (success && (partialWriteSize == numBytesWritten));
		remaining -= numBytesWritten;
		data += partialWriteSize;
	} while (success && remaining != 0);
	return lengthToWrite - size_t(remaining);
#else
	FILE* filePointer = file.p->file;
	assert(filePointer != nullptr);
	return fwrite(dataToWrite, 1, lengthToWrite, filePointer);
#endif
}

COMMON_LIBRARY_EXPORTED bool FlushFile(const WriteFileHandle& file) {
	if (file.p == nullptr) {
		return false;
	}
#ifdef _WIN32
	bool success = (FlushFileBuffers(file.p->fileHandle) != 0);
#else
	bool success = (fflush(file.p->file) == 0);
#endif
	return success;
}

COMMON_LIBRARY_EXPORTED uint64 GetFileSize(const FileHandle& file) {
	if (file.p == nullptr) {
		return 0;
	}
#ifdef _WIN32
	HANDLE fileHandle = file.p->fileHandle;
	assert(fileHandle != INVALID_HANDLE_VALUE);

#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	// GetFileSizeEx isn't available in UWP, so use GetFileInformationByHandleEx.
	FILE_STANDARD_INFO fileInfo;
	bool success = (GetFileInformationByHandleEx(fileHandle, FileStandardInfo, &fileInfo, sizeof(fileInfo)) != 0);
#else
	LARGE_INTEGER fileSizeStruct;
	bool success = (GetFileSizeEx(fileHandle, &fileSizeStruct) != 0);
#endif

	if (!success) {
		return 0;
	}

#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	uint64 fileSize = fileInfo.EndOfFile.QuadPart;
#else
	uint64 fileSize = fileSizeStruct.QuadPart;
#endif
	return fileSize;
#else
	FILE* filePointer = file.p->file;
	assert(filePointer != nullptr);

	// Save the current file offset.
	fpos_t originalFileOffset;
	bool success = (fgetpos(filePointer, &originalFileOffset) == 0);
	if (!success) {
		return 0;
	}

	success = (fseek(filePointer, 0, SEEK_END) == 0);
	if (!success) {
		return 0;
	}

	// FIXME: This won't report the correct size for large files on systems
	// where ftell returns a 32-bit integer.
	auto fileSize = ftell(filePointer);

	// Restore the original file offset.
	// In the unlikely event that this fails, it wouldn't affect the file size.
	fsetpos(filePointer, &originalFileOffset);

	return uint64(fileSize);
#endif
}

COMMON_LIBRARY_EXPORTED uint64 GetFileOffset(const FileHandle& file) {
	if (file.p == nullptr) {
		return 0;
	}
#ifdef _WIN32
	HANDLE fileHandle = file.p->fileHandle;
	assert(fileHandle != INVALID_HANDLE_VALUE);

	// There is no GetFilePointer[Ex], so we use SetFilePointerEx with
	// a distance of zero and FILE_CURRENT, in order to retrieve the
	// current file offset.
	LARGE_INTEGER distanceZero;
	distanceZero.QuadPart = 0;
	LARGE_INTEGER fileOffset;
	bool success = (SetFilePointerEx(fileHandle, distanceZero, &fileOffset, FILE_CURRENT) != 0);
	if (!success) {
		return 0;
	}
	return fileOffset.QuadPart;
#else
	FILE* filePointer = file.p->file;
	assert(filePointer != nullptr);

	// FIXME: This won't always report the correct offset for large files on systems
	// where ftell returns a 32-bit integer.
	auto fileOffset = ftell(filePointer);
	return fileOffset;
#endif
}

#ifdef _WIN32
static bool SetFileOffsetInternal(HANDLE fileHandle, int64 fileOffset, FileOffsetType type) {
	assert(fileHandle != INVALID_HANDLE_VALUE);

	DWORD setFilePointerType;
	switch (type) {
		case FileOffsetType::ABSOLUTE: {
			setFilePointerType = FILE_BEGIN;
			break;
		}
		case FileOffsetType::RELATIVE: {
			setFilePointerType = FILE_CURRENT;
			break;
		}
		case FileOffsetType::END: {
			setFilePointerType = FILE_END;
			break;
		}
		default: {
			return false;
		}
	}
	LARGE_INTEGER distance;
	distance.QuadPart = fileOffset;
	bool success = (SetFilePointerEx(fileHandle, distance, nullptr, setFilePointerType) != 0);
	return success;
}
#else
static uint64 SetFileOffsetInternal(FILE* filePointer, int64 fileOffset, FileOffsetType type) {
	assert(filePointer != nullptr);

	int fseekType;
	switch (type) {
		case FileOffsetType::ABSOLUTE: {
			fseekType = SEEK_SET;
			break;
		}
		case FileOffsetType::RELATIVE: {
			fseekType = SEEK_CUR;
			break;
		}
		case FileOffsetType::END: {
			fseekType = SEEK_END;
			break;
		}
		default: {
			return false;
		}
	}
	// FIXME: This won't always seek to the correct offset for large files on systems
	// where fseek takes a 32-bit integer.
	bool success = (fseek(filePointer, fileOffset, fseekType) == 0);
	return success;
}
#endif

COMMON_LIBRARY_EXPORTED bool SetFileOffset(const ReadFileHandle& file, int64 fileOffset, FileOffsetType type) {
	if (file.p == nullptr) {
		return false;
	}
#ifdef _WIN32
	return SetFileOffsetInternal(file.p->fileHandle, fileOffset, type);
#else
	return SetFileOffsetInternal(file.p->file, fileOffset, type);
#endif
}

COMMON_LIBRARY_EXPORTED bool SetFileOffset(const ReadWriteFileHandle& file, int64 fileOffset, FileOffsetType type) {
	if (file.p == nullptr) {
		return false;
	}
#ifdef _WIN32
	return SetFileOffsetInternal(file.p->fileHandle, fileOffset, type);
#else
	return SetFileOffsetInternal(file.p->file, fileOffset, type);
#endif
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
