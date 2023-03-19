/****
 * FileSystem.h - Provides an IFS FileSystem implementation for LittleFS.
 *
 * Created on: 8 April 2021
 *
 * This file is part of the Sming-LittleFS Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <IFS/FileSystem.h>
#include "Error.h"
#include "../../littlefs/lfs.h"
#include <memory>

namespace IFS
{
namespace LittleFS
{
// File handles start at this value
#ifndef LFS_HANDLE_MIN
#define LFS_HANDLE_MIN 200
#endif

// Maximum number of file descriptors
#ifndef LFS_MAX_FDS
#define LFS_MAX_FDS 5
#endif

// Maximum file handle value
#define LFS_HANDLE_MAX (LFS_HANDLE_MIN + LFS_MAX_FDS - 1)

constexpr size_t LFS_READ_SIZE{16};
constexpr size_t LFS_PROG_SIZE{16};
constexpr size_t LFS_BLOCK_SIZE{4096};
constexpr size_t LFS_BLOCK_CYCLES{500};
constexpr size_t LFS_CACHE_SIZE{32};
constexpr size_t LFS_LOOKAHEAD_SIZE{16};

template <typename T> constexpr lfs_attr makeAttr(AttributeTag tag, T& value)
{
	return lfs_attr{uint8_t(tag), &value, sizeof(value)};
}

struct StatAttr {
	static constexpr size_t count{5};
	struct lfs_attr attrs[count];

	StatAttr(Stat& stat)
		: attrs{
			  makeAttr(AttributeTag::ModifiedTime, stat.mtime),
			  makeAttr(AttributeTag::FileAttributes, stat.attr),
			  makeAttr(AttributeTag::ReadAce, stat.acl.readAccess),
			  makeAttr(AttributeTag::WriteAce, stat.acl.writeAccess),
			  makeAttr(AttributeTag::Compression, stat.compression),
		  }
	{
	}
};

/**
 * @brief Details for an open file
 */
struct FileDescriptor {
	CString name;
	lfs_file_t file{};
	TimeStamp mtime{};
	uint8_t buffer[LFS_CACHE_SIZE];
	struct lfs_file_config config {
		buffer
	};
	enum class Flag {
		TimeChanged,
		IsRoot,
		Write, ///< LFS throws asserts so we need to pre-check
	};
	BitSet<uint8_t, Flag, 3> flags;

	void touch()
	{
		mtime = fsGetTimeUTC();
		flags += Flag::TimeChanged;
	}
};

/**
 * Wraps LittleFS
 */
class FileSystem : public IFileSystem
{
public:
	FileSystem(Storage::Partition partition) : partition(partition)
	{
	}

	~FileSystem();

	int mount() override;
	int getinfo(Info& info) override;
	int setProfiler(IProfiler* profiler) override;
	String getErrorString(int err) override;
	int opendir(const char* path, DirHandle& dir) override;
	int readdir(DirHandle dir, Stat& stat) override;
	int rewinddir(DirHandle dir) override;
	int closedir(DirHandle dir) override;
	int mkdir(const char* path) override;
	int stat(const char* path, Stat* stat) override;
	int fstat(FileHandle file, Stat* stat) override;
	int fcontrol(FileHandle file, ControlCode code, void* buffer, size_t bufSize) override;
	int fsetxattr(FileHandle file, AttributeTag tag, const void* data, size_t size) override;
	int fgetxattr(FileHandle file, AttributeTag tag, void* buffer, size_t size) override;
	int fenumxattr(FileHandle file, AttributeEnumCallback callback, void* buffer, size_t bufsize) override;
	int setxattr(const char* path, AttributeTag tag, const void* data, size_t size) override;
	int getxattr(const char* path, AttributeTag tag, void* buffer, size_t size) override;
	FileHandle open(const char* path, OpenFlags flags) override;
	int close(FileHandle file) override;
	int read(FileHandle file, void* data, size_t size) override;
	int write(FileHandle file, const void* data, size_t size) override;
	file_offset_t lseek(FileHandle file, file_offset_t offset, SeekOrigin origin) override;
	int eof(FileHandle file) override;
	file_offset_t tell(FileHandle file) override;
	int ftruncate(FileHandle file, file_size_t new_size) override;
	int flush(FileHandle file) override;
	int rename(const char* oldpath, const char* newpath) override;
	int remove(const char* path) override;
	int fremove(FileHandle file) override;
	int format() override;
	int check() override;

private:
	int tryMount();
	void flushMeta(FileDescriptor& fd);
	void checkRootAcl(AttributeTag tag, const void* value);
	int getFileExtents(lfs_file_t& file, ExtentList* list, size_t bufSize);

	template <typename T> int get_attr(const char* path, AttributeTag tag, T& attr)
	{
		int err = lfs_getattr(&lfs, path, uint8_t(tag), &attr, sizeof(attr));
		return Error::fromSystem(err);
	}

	template <typename T> int get_attr(lfs_file_t& file, AttributeTag tag, T& attr)
	{
		int err = lfs_file_getattr(&lfs, &file, uint8_t(tag), &attr, sizeof(attr));
		return Error::fromSystem(err);
	}

	template <typename T> int set_attr(const char* path, AttributeTag tag, const T& attr)
	{
		int err = lfs_setattr(&lfs, path, uint8_t(tag), &attr, sizeof(attr));
		return Error::fromSystem(err);
	}

	template <typename T> int set_attr(lfs_file_t& file, AttributeTag tag, const T& attr)
	{
		int err = lfs_file_setattr(&lfs, &file, uint8_t(tag), &attr, sizeof(attr));
		return Error::fromSystem(err);
	}

	template <typename T> int remove_attr(const char* path, AttributeTag tag)
	{
		int err = lfs_removeattr(&lfs, path, uint8_t(tag));
		return Error::fromSystem(err);
	}

	int remove_attr(lfs_file_t& file, AttributeTag tag)
	{
		int err = lfs_file_removeattr(&lfs, &file, uint8_t(tag));
		return Error::fromSystem(err);
	}

	static int f_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size)
	{
		auto fs = static_cast<FileSystem*>(c->context);
		assert(fs != nullptr);
		uint32_t addr = (block * LFS_BLOCK_SIZE) + off;
		if(!fs->partition.read(addr, buffer, size)) {
			return LFS_ERR_IO_READ;
		}
		if(fs->profiler != nullptr) {
			fs->profiler->read(addr, buffer, size);
		}
		return LFS_ERR_OK;
	}

	static int f_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size)
	{
		auto fs = static_cast<FileSystem*>(c->context);
		assert(fs != nullptr);
		uint32_t addr = (block * LFS_BLOCK_SIZE) + off;
		if(fs->profiler != nullptr) {
			fs->profiler->write(addr, buffer, size);
		}
		return fs->partition.write(addr, buffer, size) ? LFS_ERR_OK : LFS_ERR_IO_WRITE;
	}

	static int f_erase(const struct lfs_config* c, lfs_block_t block)
	{
		auto fs = static_cast<FileSystem*>(c->context);
		assert(fs != nullptr);
		uint32_t addr = block * LFS_BLOCK_SIZE;
		size_t size = LFS_BLOCK_SIZE;
		if(fs->profiler != nullptr) {
			fs->profiler->erase(addr, size);
		}
		return fs->partition.erase_range(addr, size) ? LFS_ERR_OK : LFS_ERR_IO_ERASE;
	}

	static int f_sync(const struct lfs_config* c)
	{
		auto fs = static_cast<FileSystem*>(c->context);
		assert(fs != nullptr);
		return fs->partition.sync() ? LFS_ERR_OK : LFS_ERR_IO_WRITE;
	}

	Storage::Partition partition;
	IProfiler* profiler{nullptr};
	uint8_t readBuffer[LFS_CACHE_SIZE];
	uint8_t progBuffer[LFS_CACHE_SIZE];
	uint8_t lookaheadBuffer[LFS_LOOKAHEAD_SIZE];
	lfs_config config{
		.context = this,
		.read = f_read,
		.prog = f_prog,
		.erase = f_erase,
		.sync = f_sync,
		.read_size = LFS_READ_SIZE,
		.prog_size = LFS_PROG_SIZE,
		.block_size = LFS_BLOCK_SIZE,
		.block_count = 0,
		.block_cycles = LFS_BLOCK_CYCLES,
		.cache_size = LFS_CACHE_SIZE,
		.lookahead_size = LFS_LOOKAHEAD_SIZE,
		.read_buffer = readBuffer,
		.prog_buffer = progBuffer,
		.lookahead_buffer = lookaheadBuffer,
	};
	lfs_t lfs{};
	std::unique_ptr<FileDescriptor> fileDescriptors[LFS_MAX_FDS];
	ACL rootAcl{};
	bool mounted{false};
};

} // namespace LittleFS
} // namespace IFS
