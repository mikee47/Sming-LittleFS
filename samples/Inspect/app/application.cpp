/*
 * Webserver demo using IFS
 *
 */

#include <SmingCore.h>
#include <Data/Stream/MemoryDataStream.h>
#include <Data/Stream/IFS/DirectoryTemplate.h>
#include <LittleFS.h>
#include <Libraries/LittleFS/littlefs/lfs.h>
#include <Libraries/LittleFS/src/include/LittleFS/FileSystem.h>

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put your SSID and password here
#define WIFI_PWD "PleaseEnterPass"
#endif

namespace
{
IMPORT_FSTR(listing_txt, PROJECT_DIR "/resource/listing.txt")

void printDirectory(const char* path)
{
	auto printStream = [](IDataSourceStream& stream) {
		// Use an intermediate memory stream so debug information doesn't get mixed into output
		MemoryDataStream mem;
		mem.copyFrom(&stream);
		Serial.copyFrom(&mem);
		// Serial.copyFrom(&stream);
	};

	auto dir = new Directory;
	if(!dir->open(path)) {
		debug_e("Open '%s' failed: %s", path, dir->getLastErrorString().c_str());
		delete dir;
		return;
	}

	auto source = new FlashMemoryStream(listing_txt);
	IFS::DirectoryTemplate tmpl(source, dir);
	printStream(tmpl);
}

bool copyFiles(IFS::FileSystem& srcfs, IFS::FileSystem& dstfs, const String& path)
{
	IFS::Directory dir(&srcfs);
	if(!dir.open(path)) {
		return false;
	}

	Vector<String> directories;

	while(dir.next()) {
		auto& stat = dir.stat();
		if(stat.attr[FileAttribute::MountPoint]) {
			continue;
		}
		String filename;
		if(path.length() != 0) {
			filename += path;
			filename += '/';
		}
		filename += stat.name;
		if(stat.isDir()) {
			directories.add(filename);
			continue;
		}
		IFS::File src(&srcfs);
		if(!src.open(filename)) {
			m_printf("open('%s') failed: %s\r\n", filename.c_str(), src.getLastErrorString().c_str());
			return false;
		}
		IFS::File dst(&dstfs);
		if(!dst.open(filename, File::CreateNewAlways | File::WriteOnly)) {
			m_printf("create('%s') failed: %s\r\n", filename.c_str(), dst.getLastErrorString().c_str());
			return false;
		}
		src.readContent([&dst](const char* buffer, size_t size) -> int { return dst.write(buffer, size); });
		int err = dst.getLastError();
		if(err < 0) {
			m_printf("Copy write '%s' failed: %s\r\n", filename.c_str(), dst.getLastErrorString().c_str());
			return false;
		}
		err = src.getLastError();
		if(err < 0) {
			m_printf("Copy read '%s' failed: %s\r\n", filename.c_str(), src.getLastErrorString().c_str());
			return false;
		}
		if(!dst.settime(stat.mtime)) {
			m_printf("settime('%s') failed: %s", filename.c_str(), dst.getLastErrorString().c_str());
			return false;
		}
		if(!dst.setcompression(stat.compression)) {
			m_printf("setcompression('%s') failed: %s\r\n", filename.c_str(), dst.getLastErrorString().c_str());
			return false;
		}
		if(!dst.setacl(stat.acl)) {
			m_printf("setacl('%s') failed: %s\r\n", filename.c_str(), dst.getLastErrorString().c_str());
			return false;
		}
	}
	dir.close();

	for(auto& dirname : directories) {
		int err = dstfs.mkdir(dirname);
		if(err < 0) {
			m_printf("mkdir('%s') failed: %s\r\n", dirname.c_str(), dstfs.getErrorString(err).c_str());
			return false;
		}
		if(!copyFiles(srcfs, dstfs, dirname)) {
			return false;
		}
	}

	return true;
}

bool copyFileSystem(IFS::FileSystem* srcfs, IFS::FileSystem* dstfs)
{
	bool res = copyFiles(*srcfs, *dstfs, nullptr);

	IFS::FileSystem::Info info;
	dstfs->getinfo(info);
	auto used = info.volumeSize - info.freeSpace;

	auto sizeToMb = [](size_t size) { return String(size / 1024.0 / 1024.0) + "MB"; };

	m_printf("Used space: 0x%08x (%s), Free space: 0x%08x (%s)\r\n", used, sizeToMb(used).c_str(), info.freeSpace,
			 sizeToMb(info.freeSpace).c_str());

	return res;
}

void copySomeFiles()
{
	auto part = *Storage::findPartition(Storage::Partition::SubType::Data::fwfs);
	if(!part) {
		return;
	}
	auto fs = IFS::createFirmwareFilesystem(part);
	if(fs == nullptr) {
		return;
	}
	fs->mount();
	copyFileSystem(fs, getFileSystem());
	delete fs;
}

bool isVolumeEmpty()
{
	Directory dir;
	dir.open();
	return !dir.next();
}

void fstest()
{
	lfs_mount();

	IFS::Profiler profiler;
	getFileSystem()->setProfiler(&profiler);

	const char str[]{"This is a test attribute, should be at number 10"};
	getFileSystem()->setxattr("readme.md", IFS::getUserAttributeTag(10), str, sizeof(str));

	// getFileSystem()->removeattrtag("readme.md", IFS::getUserAttributeTag(10));
	int err = getFileSystem()->getxattr("readme.md", IFS::getUserAttributeTag(10), nullptr, 0);
	debug_w("getxattr(): %d", err);

	if(isVolumeEmpty()) {
		Serial.print(F("Volume appears to be empty, writing some files...\r\n"));
		copySomeFiles();
	}

	printDirectory(nullptr);

	getFileSystem()->setProfiler(nullptr);

	Serial.print(F("Perf stats: "));
	Serial.println(profiler.toString());

	auto kb = [](size_t size) {
		String s;
		s += (size + 1023) / 1024;
		s += "KB";
		return s;
	};

	IFS::FileSystem::Info info;
	getFileSystem()->getinfo(info);
	Serial.print(F("Volume Size: "));
	Serial.print(kb(info.volumeSize));
	Serial.print(F(", Used: "));
	Serial.print(kb(info.used()));
	Serial.print(F(", Free space: "));
	Serial.print(kb(info.freeSpace));
	Serial.println();
}

void swap(uint32_t& value)
{
	value = __builtin_bswap32(value);
}

void readStructure(Storage::Partition part, const lfs_block_t pair[2])
{
	uint32_t rev[2];
	part.read(pair[0] * IFS::LittleFS::LFS_BLOCK_SIZE, rev[0]);
	part.read(pair[1] * IFS::LittleFS::LFS_BLOCK_SIZE, rev[1]);
	lfs_block_t block = (rev[1] > rev[0]) ? pair[1] : pair[0];
	lfs_off_t off = block * IFS::LittleFS::LFS_BLOCK_SIZE;
	off += 4; // revision
	uint32_t prevtag = 0xffffffff;
	for(;;) {
		uint32_t xtag;
		if(!part.read(off, xtag)) {
			break;
		}
		swap(xtag);
		union {
			uint32_t value;
			struct {
				uint32_t size : 10;
				uint32_t id : 10;
				uint32_t type : 11;
				uint32_t valid : 1;
			};
		} tag{xtag ^ prevtag};
		prevtag = tag.value;

		if(tag.valid) {
			m_printf("0x%08x: END\r\n\r\n", off);
			break;
		}

		char s[128];
		m_snprintf(s, sizeof(s), "0x%08x: tag=0x%08x, type=0x%03x, id=0x%03x, size=0x%03x, data", off, tag.value,
				   tag.type, tag.id, tag.size);
		off += sizeof(xtag);
		if(tag.size == 0x3ff) {
			// Special value: Tag has been deleted
			tag.size = 0;
		}
		uint8_t buf[0x400];
		if(!part.read(off, buf, tag.size)) {
			break;
		}
		m_printHex(s, buf, std::min(tag.size, 128U));
		off += tag.size;

		continue;
		if(tag.type == LFS_TYPE_SOFTTAIL) {
			lfs_block_t pair[2];
			memcpy(pair, buf, sizeof(pair));
			readStructure(part, pair);
		}
	}
}

void test()
{
	auto part = Storage::findDefaultPartition(Storage::Partition::SubType::Data::littlefs);
	uint32_t pair[]{0, 1};
	readStructure(part, pair);
}

} // namespace

void init()
{
#if DEBUG_BUILD
	Serial.begin(COM_SPEED_SERIAL);
	Serial.systemDebugOutput(true);
#endif

	test();

	// Delay at startup so terminal gets time to start
	auto timer = new AutoDeleteTimer;
	timer->initializeMs<1000>(fstest);
	timer->startOnce();
}
