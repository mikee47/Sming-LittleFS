/**
 * FileSystem.cpp
 *
 * Created on: 8 April 2021
 *
 * Copyright 2021 mikee47 <mike@sillyhouse.net>
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
 ****/

#include "include/LittleFS/FileSystem.h"
#include "include/LittleFS/Error.h"
#include <IFS/Util.h>

namespace
{
#include "debug.h"
}

namespace IFS
{
/**
 * @brief LittleFS directory object
 */
struct FileDir {
	lfs_dir_t dir;
};

namespace LittleFS
{
namespace
{
void fillStat(Stat& stat, const lfs_info& info)
{
	auto name = info.name;
	if(*name == '/') {
		++name;
	}
	stat.name.copy(name);
	stat.size = info.size;

	if(info.type == LFS_TYPE_DIR) {
		stat.attr += FileAttribute::Directory;
	}
}

} // namespace

/**
 * @brief map IFS OpenFlags to LFS equivalents
 * @param flags
 * @param sflags OUT the LFS file open flags
 * @retval OpenFlags if non-zero then some flags weren't recognised
 */
OpenFlags mapFileOpenFlags(OpenFlags flags, lfs_open_flags& lfsflags)
{
	uint32_t oflags = 0;

	auto map = [&](OpenFlag flag, lfs_open_flags oflag) {
		if(flags[flag]) {
			oflags |= oflag;
			flags -= flag;
		}
	};

	map(OpenFlag::Append, LFS_O_APPEND);
	map(OpenFlag::Truncate, LFS_O_TRUNC);
	map(OpenFlag::Create, LFS_O_CREAT);
	map(OpenFlag::Read, LFS_O_RDONLY);
	map(OpenFlag::Write, LFS_O_WRONLY);

	if(flags.any()) {
		debug_w("Unknown OpenFlags: 0x%02X", flags.value());
	}

	lfsflags = lfs_open_flags(oflags);
	return flags;
}

#define GET_FD()                                                                                                       \
	if(file < LFS_HANDLE_MIN || file > LFS_HANDLE_MAX) {                                                               \
		return Error::InvalidHandle;                                                                                   \
	}                                                                                                                  \
	auto& fd = fileDescriptors[file - LFS_HANDLE_MIN];                                                                 \
	if(fd == nullptr) {                                                                                                \
		return Error::FileNotOpen;                                                                                     \
	}

FileSystem::~FileSystem()
{
	lfs_unmount(&lfs);
}

int FileSystem::mount()
{
	if(!partition) {
		return Error::NoPartition;
	}

	if(!partition.verify(Storage::Partition::SubType::Data::littlefs)) {
		return Error::BadPartition;
	}

	config.context = &partition;
	config.block_count = partition.size() / LFS_BLOCK_SIZE;

	auto res = tryMount();
	if(res < 0) {
		/*
		 * Mount failed, so we either try to repair the system or format it.
		 * For now, just format it.
		 */
		debug_w("[LFS] Mount failed, formatting");
		res = format();
	}

	return res;
}

int FileSystem::tryMount()
{
	assert(!mounted);
	lfs = lfs_t{};
	auto err = lfs_mount(&lfs, &config);
	if(err < 0) {
		err = Error::fromSystem(err);
		debug_ifserr(err, "lfs_mount()");
	} else {
		mounted = true;
	}

	return err;
}

/*
 * Format the file system and leave it mounted in an accessible state.
 */
int FileSystem::format()
{
	if(mounted) {
		lfs_unmount(&lfs);
		mounted = false;
	}
	lfs = lfs_t{};
	int err = lfs_format(&lfs, &config);
	if(err < 0) {
		err = Error::fromSystem(err);
		debug_ifserr(err, "format()");
		return err;
	}

	// Re-mount
	return tryMount();
}

int FileSystem::check()
{
	return Error::NotImplemented;
}

int FileSystem::getinfo(Info& info)
{
	info.clear();
	info.partition = partition;
	info.type = Type::LittleFS;
	info.maxNameLength = LFS_NAME_MAX;
	info.maxPathLength = UINT16_MAX;
	if(mounted) {
		info.attr |= Attribute::Mounted;
		auto usedBlocks = lfs_fs_size(&lfs);
		if(usedBlocks < 0) {
			return Error::fromSystem(usedBlocks);
		}
		info.volumeSize = usedBlocks * LFS_BLOCK_SIZE;
		info.freeSpace = (config.block_count - usedBlocks) * LFS_BLOCK_SIZE;
	}

	return FS_OK;
}

String FileSystem::getErrorString(int err)
{
	if(Error::isSystem(err)) {
		return lfsErrorToStr(Error::toSystem(err));
	} else {
		return IFS::Error::toString(err);
	}
}

FileHandle FileSystem::fopen(const Stat& stat, OpenFlags flags)
{
	/*
	 * TODO: Requires littlefs library update to support opening a file by it's ID.
	 */
	debug_w("Warning: LittleFS doesn't currently support opening by stat/ID");
	return Error::NotImplemented;
}

FileHandle FileSystem::open(const char* path, OpenFlags flags)
{
	FS_CHECK_PATH(path);
	if(path == nullptr) {
		return Error::BadParam;
	}

	// If file is marked read-only, fail write requests
	if(flags[OpenFlag::Write]) {
		FileAttributes attr;
		get_attr(path, AttributeTag::FileAttributes, attr);
		if(attr[FileAttribute::ReadOnly]) {
			return Error::ReadOnly;
		}
	}

	lfs_open_flags oflags;
	if(mapFileOpenFlags(flags, oflags).any()) {
		return FileHandle(Error::NotSupported);
	}

	/*
	 * Allocate a file descriptor
	 */
	int file{Error::OutOfFileDescs};
	for(unsigned i = 0; i < LFS_MAX_FDS; ++i) {
		auto& fd = fileDescriptors[i];
		if(!fd) {
			fd.reset(new FileDescriptor);
			file = LFS_HANDLE_MIN + i;
			break;
		}
	}
	if(file < 0) {
		debug_ifserr(file, "open('%s')", path);
		return file;
	}

	auto& fd = fileDescriptors[file - LFS_HANDLE_MIN];
	int err = lfs_file_opencfg(&lfs, &fd->file, path, oflags, &fd->config);
	if(err < 0) {
		err = Error::fromSystem(err);
		debug_ifserr(err, "open('%s')", path);
		fd.reset();
		return err;
	}

	get_attr(fd->file, AttributeTag::ModifiedTime, fd->mtime);
	get_attr(fd->file, AttributeTag::Acl, fd->acl);
	get_attr(fd->file, AttributeTag::Compression, fd->compression);
	get_attr(fd->file, AttributeTag::FileAttributes, fd->attr);

	// Copy name into descriptor
	if(path != nullptr) {
		const char* p = strrchr(path, '/');
		if(p == nullptr) {
			p = path;
		} else {
			++p;
		}
		fd->name = p;
	}

	return file;
}

int FileSystem::close(FileHandle file)
{
	GET_FD()

	flushMeta(*fd);

	int res = lfs_file_close(&lfs, &fd->file);
	fd.reset();
	return Error::fromSystem(res);
}

int FileSystem::eof(FileHandle file)
{
	GET_FD()

	auto size = lfs_file_size(&lfs, &fd->file);
	if(size < 0) {
		return Error::fromSystem(size);
	}
	auto pos = lfs_file_tell(&lfs, &fd->file);
	if(pos < 0) {
		return Error::fromSystem(pos);
	}
	return (pos >= size) ? 1 : 0;
}

int32_t FileSystem::tell(FileHandle file)
{
	GET_FD()

	int res = lfs_file_tell(&lfs, &fd->file);
	return Error::fromSystem(res);
}

int FileSystem::ftruncate(FileHandle file, size_t new_size)
{
	GET_FD()

	int res = lfs_file_truncate(&lfs, &fd->file, new_size);
	return Error::fromSystem(res);
}

void FileSystem::flushMeta(FileDescriptor& fd)
{
	flush_attr(fd, AttributeTag::ModifiedTime, fd.mtime);
	flush_attr(fd, AttributeTag::FileAttributes, fd.attr);
	flush_attr(fd, AttributeTag::Compression, fd.compression);
	flush_attr(fd, AttributeTag::Acl, fd.acl);
}

int FileSystem::flush(FileHandle file)
{
	GET_FD()

	flushMeta(*fd);

	int res = lfs_file_sync(&lfs, &fd->file);
	return Error::fromSystem(res);
}

int FileSystem::read(FileHandle file, void* data, size_t size)
{
	GET_FD()

	int res = lfs_file_read(&lfs, &fd->file, data, size);
	if(res < 0) {
		int err = Error::fromSystem(res);
		debug_ifserr(err, "read()");
		return err;
	}

	return res;
}

int FileSystem::write(FileHandle file, const void* data, size_t size)
{
	GET_FD()

	int res = lfs_file_write(&lfs, &fd->file, data, size);
	if(res < 0) {
		return Error::fromSystem(res);
	}

	fd->touch();
	return res;
}

int FileSystem::lseek(FileHandle file, int offset, SeekOrigin origin)
{
	GET_FD()

	int res = lfs_file_seek(&lfs, &fd->file, offset, int(origin));
	return Error::fromSystem(res);
}

int FileSystem::stat(const char* path, Stat* stat)
{
	if(stat == nullptr) {
		struct lfs_info info;
		int err = lfs_stat(&lfs, path ?: "", &info);
		return Error::fromSystem(err);
	}

	*stat = Stat{};
	StatAttr sa(*stat);
	struct lfs_stat_config cfg {
		sa.attrs, sa.count
	};
	struct lfs_info info;
	int err = lfs_statcfg(&lfs, path ?: "", &info, &cfg);
	if(err < 0) {
		return Error::fromSystem(err);
	}

	stat->fs = this;
	if(stat->compression.type != Compression::Type::None) {
		stat->attr += FileAttribute::Compressed;
	}
	fillStat(*stat, info);
	return FS_OK;
}

int FileSystem::fstat(FileHandle file, Stat* stat)
{
	GET_FD()

	auto size = lfs_file_size(&lfs, &fd->file);
	if(stat == nullptr || size < 0) {
		return Error::fromSystem(size);
	}

	*stat = Stat{};
	stat->fs = this;
	stat->id = fd->file.id;
	/*
	* TODO: Update littlefs library so we can query name of open file ?
	*/
	stat->name.copy(fd->name.c_str());
	stat->size = size;
	stat->mtime = fd->mtime;
	stat->attr = fd->attr;
	stat->acl = fd->acl;
	stat->compression = fd->compression;

	return FS_OK;
}

int FileSystem::fsetxattr(FileHandle file, AttributeTag tag, const void* data, size_t size)
{
	GET_FD()

	if(data == nullptr) {
		// Cannot delete standard attributes
		if(tag < AttributeTag::UserStart) {
			return Error::NotSupported;
		}
		int err = lfs_file_removeattr(&lfs, &fd->file, uint8_t(tag));
		return Error::fromSystem(err);
	}

	auto attrSize = getAttributeSize(tag);
	if(size < attrSize) {
		return Error::BadParam;
	}
	auto value = fd->getattr(tag);
	if(value != nullptr) {
		if(memcmp(value, data, attrSize) != 0) {
			memcpy(value, data, attrSize);
			fd->dirty += tag;
		}
		return FS_OK;
	}
	return lfs_file_setattr(&lfs, &fd->file, uint8_t(tag), data, size);
}

int FileSystem::fgetxattr(FileHandle file, AttributeTag tag, void* buffer, size_t size)
{
	GET_FD()

	auto attrSize = getAttributeSize(tag);
	if(size >= attrSize) {
		auto value = fd->getattr(tag);
		if(value != nullptr) {
			memcpy(buffer, &value, attrSize);
			return attrSize;
		}
		return lfs_file_getattr(&lfs, &fd->file, uint8_t(tag), buffer, size);
	}
	return attrSize;
}

int FileSystem::setxattr(const char* path, AttributeTag tag, const void* data, size_t size)
{
	if(data == nullptr) {
		// Cannot delete standard attributes
		if(tag < AttributeTag::UserStart) {
			return Error::NotSupported;
		}
		int err = lfs_removeattr(&lfs, path, uint8_t(tag));
		return Error::fromSystem(err);
	}

	auto attrSize = getAttributeSize(tag);
	if(size < attrSize) {
		return Error::BadParam;
	}
	FS_CHECK_PATH(path);
	int err = lfs_setattr(&lfs, path ?: "", uint8_t(tag), data, size);
	return Error::fromSystem(err);
}

int FileSystem::getxattr(const char* path, AttributeTag tag, void* buffer, size_t size)
{
	auto attrSize = getAttributeSize(tag);
	if(size < attrSize) {
		return attrSize;
	}
	FS_CHECK_PATH(path);
	int res = lfs_getattr(&lfs, path ?: "", uint8_t(tag), buffer, size);
	return Error::fromSystem(res);
}

int FileSystem::opendir(const char* path, DirHandle& dir)
{
	FS_CHECK_PATH(path);

	auto d = new FileDir{};
	if(d == nullptr) {
		return Error::NoMem;
	}

	int err = lfs_dir_open(&lfs, &d->dir, path ?: "");
	if(err < 0) {
		err = Error::fromSystem(err);
		debug_ifserr(err, "opendir");
		delete d;
		return err;
	}
	lfs_dir_seek(&lfs, &d->dir, 2);

	dir = d;
	return FS_OK;
}

int FileSystem::rewinddir(DirHandle dir)
{
	if(dir == nullptr) {
		return Error::BadParam;
	}

	// Skip "." and ".." entries for consistency with other filesystems
	int err = lfs_dir_seek(&lfs, &dir->dir, 2);
	return Error::fromSystem(err);
}

int FileSystem::readdir(DirHandle dir, Stat& stat)
{
	if(dir == nullptr) {
		return Error::BadParam;
	}

	stat = Stat{};
	StatAttr sa(stat);
	struct lfs_stat_config cfg {
		sa.attrs, sa.count
	};
	struct lfs_info info;
	int err = lfs_dir_readcfg(&lfs, &dir->dir, &info, &cfg);
	if(err == 0) {
		return Error::NoMoreFiles;
	}
	if(err < 0) {
		return Error::fromSystem(err);
	}

	debug_e("DIR %s: %u, %u, %s", info.name, info.type, info.size, dir_str(dir->dir).c_str());

	stat.fs = this;
	stat.id = dir->dir.id - 1;
	if(stat.compression.type != Compression::Type::None) {
		stat.attr += FileAttribute::Compressed;
	}
	fillStat(stat, info);
	return FS_OK;
}

int FileSystem::closedir(DirHandle dir)
{
	if(dir == nullptr) {
		return Error::BadParam;
	}

	int err = lfs_dir_close(&lfs, &dir->dir);
	delete dir;
	return Error::fromSystem(err);
}

int FileSystem::mkdir(const char* path)
{
	int err = lfs_mkdir(&lfs, path);
	if(err == 0) {
		TimeStamp mtime;
		mtime = fsGetTimeUTC();
		set_attr(path, AttributeTag::ModifiedTime, mtime);
	}
	return Error::fromSystem(err);
}

int FileSystem::rename(const char* oldpath, const char* newpath)
{
	FS_CHECK_PATH(oldpath);
	FS_CHECK_PATH(newpath);
	if(oldpath == nullptr || newpath == nullptr) {
		return Error::BadParam;
	}

	int err = lfs_rename(&lfs, oldpath, newpath);
	return Error::fromSystem(err);
}

int FileSystem::remove(const char* path)
{
	FS_CHECK_PATH(path);
	if(path == nullptr) {
		return Error::BadParam;
	}

	// Check file is not marked read-only
	FileAttributes attr;
	int err = get_attr(path, AttributeTag::FileAttributes, attr);
	if(err < 0) {
		return err;
	}
	if(attr[FileAttribute::ReadOnly]) {
		return Error::ReadOnly;
	}

	err = lfs_remove(&lfs, path);
	return Error::fromSystem(err);
}

int FileSystem::fremove(FileHandle file)
{
	GET_FD()

	if(fd->attr[FileAttribute::ReadOnly]) {
		return Error::ReadOnly;
	}

	/*
	 * TODO: Update littlefs library to support deletion of open file.
	 * Note that we can mark the file descriptor as invalid, but don't release it:
	 * that happens when the user calls `close()`.
	 */
	return Error::NotImplemented;
}

} // namespace LittleFS
} // namespace IFS
