#include <SmingCore.h>
#include <LittleFS.h>
#include <Libraries/LittleFS/littlefs/lfs.h>
#include <Libraries/LittleFS/src/include/LittleFS/FileSystem.h>
#include <IFS/FileCopier.h>
#include <IFS/Debug.h>

namespace
{
void copySomeFiles()
{
	auto part = *Storage::findPartition(Storage::Partition::SubType::Data::fwfs);
	if(!part) {
		return;
	}
	std::unique_ptr<IFS::FileSystem> fs{IFS::createFirmwareFilesystem(part)};
	if(!fs) {
		return;
	}
	fs->mount();

	IFS::FileCopier copier(*fs, *getFileSystem());
	copier.copyDir(nullptr, nullptr);
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

	auto fs = getFileSystem();

	IFS::Profiler profiler;
	fs->setProfiler(&profiler);

	const char str[]{"This is a test attribute, should be at number 10"};
	fs->setxattr("readme.md", IFS::getUserAttributeTag(10), str, sizeof(str));

	// fs->removeattrtag("readme.md", IFS::getUserAttributeTag(10));
	int err = fs->getxattr("readme.md", IFS::getUserAttributeTag(10), nullptr, 0);
	debug_w("getxattr(): %d", err);

	if(isVolumeEmpty()) {
		Serial.print(F("Volume appears to be empty, writing some files...\r\n"));
		copySomeFiles();
	}

	IFS::Debug::listDirectory(Serial, *fs, nullptr);

	fs->setProfiler(nullptr);

	Serial << _F("Perf stats: ") << profiler << endl;

	auto kb = [](volume_size_t size) { return (size + 1023) / 1024; };

	IFS::FileSystem::Info info;
	fs->getinfo(info);
	Serial << F("Volume Size: ") << kb(info.volumeSize) << F(" KB, Used: ") << kb(info.used()) << F(" KB, Free space: ")
		   << kb(info.freeSpace) << " KB" << endl;
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
			m_printf("0x%08x: END\r\n\r\n", unsigned(off));
			break;
		}

		char s[128];
		m_snprintf(s, sizeof(s), "0x%08x: tag=0x%08x, type=0x%03x, id=0x%03x, size=0x%03x, data", unsigned(off),
				   unsigned(tag.value), tag.type, tag.id, tag.size);
		off += sizeof(xtag);
		if(tag.size == 0x3ff) {
			// Special value: Tag has been deleted
			tag.size = 0;
		}
		uint8_t buf[0x400];
		if(!part.read(off, buf, tag.size)) {
			break;
		}
		m_printHex(s, buf, std::min(tag.size, uint32_t(128)));
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
	Serial.begin(COM_SPEED_SERIAL);
	Serial.systemDebugOutput(true);

	test();

	fstest();
}
