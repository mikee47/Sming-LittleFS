#include <SmingCore.h>
#include <LittleFS.h>
#include <Storage/FileDevice.h>
#include <hostlib/CommandLine.h>
#include <memory>

bool copyFiles(IFS::FileSystem& srcfs, IFS::FileSystem& dstfs, const String& path)
{
	IFS::Directory dir(&srcfs);
	if(!dir.open(path)) {
		return false;
	}

	struct Dir {
		String path;
		IFS::TimeStamp mtime;
	};
	Vector<Dir> directories;

	while(dir.next()) {
		auto& stat = dir.stat();
		String filename;
		if(path.length() != 0) {
			filename += path;
			filename += '/';
		}
		filename += stat.name;
		if(stat.isDir()) {
			directories.add(Dir{filename, stat.mtime});
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

	auto time = IFS::fsGetTimeUTC();

	for(auto& dir : directories) {
		int err = dstfs.mkdir(dir.path);
		if(err < 0) {
			m_printf("mkdir('%s') failed: %s\r\n", dir.path.c_str(), dstfs.getErrorString(err).c_str());
			return false;
		}
		if(dir.mtime != time) {
			dstfs.settime(dir.path, dir.mtime);
		}
		if(!copyFiles(srcfs, dstfs, dir.path)) {
			return false;
		}
	}

	return true;
}

bool fscopy(const char* srcFile, const char* dstFile, size_t dstSize)
{
	auto& hostfs = IFS::Host::getFileSystem();

	// Source
	auto file = hostfs.open(srcFile, File::ReadOnly);
	if(file < 0) {
		m_printf("open('%s') failed: %s\r\n", srcFile, hostfs.getErrorString(file).c_str());
		return false;
	}
	Storage::FileDevice srcDevice("SRC", hostfs, file);
	auto part = srcDevice.createPartition("src", Storage::Partition::SubType::Data::fwfs, 0, hostfs.getSize(file),
										  Storage::Partition::Flag::readOnly);
	auto srcfs = IFS::createFirmwareFilesystem(part);
	srcfs->mount();

	// Destination
	file = hostfs.open(dstFile, File::CreateNewAlways | File::ReadWrite);
	if(file < 0) {
		m_printf("open('%s') failed: %s\r\n", dstFile, hostfs.getErrorString(file).c_str());
		delete srcfs;
		return false;
	}

	// Fill destination with FF
	uint8_t buffer[512];
	memset(buffer, 0xff, sizeof(buffer));
	for(size_t off = 0; off < dstSize; off += sizeof(buffer)) {
		hostfs.write(file, buffer, sizeof(buffer));
	}
	Storage::FileDevice dstDevice("DST", hostfs, file);
	part = dstDevice.createPartition("dst", Storage::Partition::SubType::Data::littlefs, 0, dstSize);
	auto dstfs = IFS::createLfsFilesystem(part);
	dstfs->mount();

	//
	bool res = copyFiles(*IFS::FileSystem::cast(srcfs), *IFS::FileSystem::cast(dstfs), nullptr);

	IFS::FileSystem::Info info;
	dstfs->getinfo(info);
	auto used = info.volumeSize - info.freeSpace;

	auto sizeToMb = [](size_t size) { return String(size / 1024.0 / 1024.0) + "MB"; };

	m_printf("Used space: 0x%08x (%s), Free space: 0x%08x (%s)\r\n", used, sizeToMb(used).c_str(), info.freeSpace,
			 sizeToMb(info.freeSpace).c_str());

	delete dstfs;
	delete srcfs;

	return res;
}

void init()
{
	// Hook up debug output
	Serial.begin(COM_SPEED_SERIAL);
	Serial.systemDebugOutput(true);

	auto parameters = commandLine.getParameters();
	if(parameters.count() != 3) {
		m_printf("Usage: fscopy <source file> <dest file> <dest size>\r\n");
	} else {
		auto size = strtoul(parameters[2].text, nullptr, 0);
		if(!fscopy(parameters[0].text, parameters[1].text, size)) {
			abort();
		}
	}
	System.restart();
}
