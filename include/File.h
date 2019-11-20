#pragma once

// This file contains declarations of functions for reading/writing files.

#include "Types.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This returns true if the whole file was able to be read into the contents array, else false.
COMMON_LIBRARY_EXPORTED bool ReadWholeFile(const char* filename, Array<char>& contents);

// This returns true if the whole contents array was able to be written into the file, else false.
COMMON_LIBRARY_EXPORTED bool WriteWholeFile(const char* filename, const void* contents, size_t length);

class COMMON_LIBRARY_EXPORTED FileHandle {
protected:
	// Internal implementation is platform-dependent.
	class InternalHandle;
	InternalHandle* p;

	// For internal use only
	INLINE FileHandle(InternalHandle* p_) noexcept : p(p_) {}
	// Increments the reference count of p.
	void incrementRefCount() noexcept;
	// Decrements the reference count of p and deletes it if it becomes zero.
	void decrementRefCount() noexcept;

	friend COMMON_LIBRARY_EXPORTED uint64 GetFileSize(const FileHandle&);
	friend COMMON_LIBRARY_EXPORTED uint64 GetFileOffset(const FileHandle&);
public:
	INLINE FileHandle() noexcept : p(nullptr) {}
	INLINE ~FileHandle() noexcept {
		if (p != nullptr) {
			decrementRefCount();
		}
	}
	INLINE FileHandle(FileHandle&& that) noexcept : p(that.p) {
		that.p = nullptr;
	}
	INLINE FileHandle(const FileHandle& that) noexcept : p(that.p) {
		if (p != nullptr) {
			incrementRefCount();
		}
	}
	FileHandle& operator=(FileHandle&& that) noexcept {
		if (p != that.p) {
			if (p != nullptr) {
				decrementRefCount();
			}
			p = that.p;
			that.p = nullptr;
		}
		return *this;
	}
	FileHandle& operator=(const FileHandle& that) noexcept {
		if (p != that.p) {
			if (p != nullptr) {
				decrementRefCount();
			}
			p = that.p;
			if (p != nullptr) {
				incrementRefCount();
			}
		}
		return *this;
	}

	INLINE bool operator==(const FileHandle& that) const noexcept {
		return (p == that.p);
	}
	INLINE bool operator!=(const FileHandle& that) const noexcept {
		return !(*this == that);
	}

	void clear() noexcept {
		if (p != nullptr) {
			decrementRefCount();
			p = nullptr;
		}
	}

	INLINE bool isClear() const noexcept {
		return (p == nullptr);
	}

	INLINE bool operator!() const noexcept {
		return (p == nullptr);
	}
	INLINE explicit operator bool() const noexcept {
		return (p != nullptr);
	}

	size_t getRefCount() noexcept;
};

enum class FileOffsetType {
	// bytes from the beginning
	ABSOLUTE,
	// bytes from the current file offset
	RELATIVE,
	// bytes relative to end (negative for before end)
	END
};

// A ReadFileHandle can read and seek.
class COMMON_LIBRARY_EXPORTED ReadFileHandle : public FileHandle {
	using FileHandle::FileHandle;
	friend COMMON_LIBRARY_EXPORTED ReadFileHandle OpenFileRead(const char*);
	friend COMMON_LIBRARY_EXPORTED size_t ReadFile(const ReadFileHandle&, void*, size_t);
	friend COMMON_LIBRARY_EXPORTED bool SetFileOffset(const ReadFileHandle&, int64, FileOffsetType);
};

// A WriteFileHandle can write, but not read or seek, e.g. for appending.
class COMMON_LIBRARY_EXPORTED WriteFileHandle : public FileHandle {
	using FileHandle::FileHandle;
	friend COMMON_LIBRARY_EXPORTED WriteFileHandle CreateFile(const char*);
	friend COMMON_LIBRARY_EXPORTED WriteFileHandle OpenFileAppend(const char*);
	friend COMMON_LIBRARY_EXPORTED size_t WriteFile(const WriteFileHandle&, const void*, size_t);
	friend COMMON_LIBRARY_EXPORTED bool FlushFile(const WriteFileHandle& file);
};

// A ReadWriteFileHandle can read, write, and seek.
class COMMON_LIBRARY_EXPORTED ReadWriteFileHandle : public WriteFileHandle {
	using WriteFileHandle::WriteFileHandle;
	friend COMMON_LIBRARY_EXPORTED ReadWriteFileHandle OpenFileReadWrite(const char*);
	friend COMMON_LIBRARY_EXPORTED size_t ReadFile(const ReadWriteFileHandle&, void*, size_t);
	friend COMMON_LIBRARY_EXPORTED bool SetFileOffset(const ReadWriteFileHandle&, int64, FileOffsetType);
};

// FileHandle does not contain self-pointers, so can be realloc'd.
template<> struct is_trivially_relocatable<FileHandle> : public std::true_type {};
template<> struct is_trivially_relocatable<ReadFileHandle> : public std::true_type {};
template<> struct is_trivially_relocatable<WriteFileHandle> : public std::true_type {};
template<> struct is_trivially_relocatable<ReadWriteFileHandle> : public std::true_type {};

// Creates a new file for appending, replacing any existing file
COMMON_LIBRARY_EXPORTED WriteFileHandle CreateFile(const char* filename);

// Opens an existing file for reading
COMMON_LIBRARY_EXPORTED ReadFileHandle OpenFileRead(const char* filename);

// Opens a file for appending, starting the file pointer at the end, creating
// a new file if one didn't already exist.
COMMON_LIBRARY_EXPORTED WriteFileHandle OpenFileAppend(const char* filename);

// Opens a file for reading and writing. creating a new file if one didn't
// already exist
COMMON_LIBRARY_EXPORTED ReadWriteFileHandle OpenFileReadWrite(const char* filename);

COMMON_LIBRARY_EXPORTED size_t ReadFile(const ReadFileHandle& file, void* dataFromFile, size_t lengthToRead);
COMMON_LIBRARY_EXPORTED size_t ReadFile(const ReadWriteFileHandle& file, void* dataFromFile, size_t lengthToRead);
COMMON_LIBRARY_EXPORTED size_t WriteFile(const WriteFileHandle& file, const void* dataToWrite, size_t lengthToWrite);
COMMON_LIBRARY_EXPORTED bool FlushFile(const WriteFileHandle& file);

COMMON_LIBRARY_EXPORTED uint64 GetFileSize(const FileHandle& file);

COMMON_LIBRARY_EXPORTED uint64 GetFileOffset(const FileHandle& file);

COMMON_LIBRARY_EXPORTED bool SetFileOffset(const ReadFileHandle& file, int64 fileOffset, FileOffsetType type = FileOffsetType::ABSOLUTE);
COMMON_LIBRARY_EXPORTED bool SetFileOffset(const ReadWriteFileHandle& file, int64 fileOffset, FileOffsetType type = FileOffsetType::ABSOLUTE);

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
