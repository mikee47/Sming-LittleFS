#include <SmingCore.h>
#include <LittleFS.h>
#include <Storage/FileDevice.h>
#include <hostlib/CommandLine.h>
#include <memory>

namespace
{
IFS::FileSystem* srcfs;
IFS::FileSystem* dstfs;

bool error(const String& errstr, const char* operation, const String& arg)
{
	m_printf("%s('%s') failed: %s\r\n", operation, arg.c_str(), errstr.c_str());
	return false;
}

bool error(IFS::FsBase& obj, const char* operation, const String& arg)
{
	return error(obj.getLastErrorString(), operation, arg);
}

bool copyFiles(const String& path)
{
	IFS::Directory dir(srcfs);
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
		IFS::File src(srcfs);
		if(!src.open(filename)) {
			return error(src, "open", filename);
		}
		IFS::File dst(dstfs);
		if(!dst.open(filename, File::CreateNewAlways | File::WriteOnly)) {
			return error(dst, "create", filename);
		}
		src.readContent([&dst](const char* buffer, size_t size) -> int { return dst.write(buffer, size); });
		int err = dst.getLastError();
		if(err < 0) {
			return error(dst, "write", filename);
		}
		err = src.getLastError();
		if(err < 0) {
			return error(src, "read", filename);
		}
		if(!dst.settime(stat.mtime)) {
			return error(dst, "settime", filename);
		}
		if(!dst.setcompression(stat.compression)) {
			return error(dst, "setcompression", filename);
		}
		if(!dst.setacl(stat.acl)) {
			return error(dst, "setacl", filename);
		}
	}
	dir.close();

	auto time = IFS::fsGetTimeUTC();

	for(auto& dir : directories) {
		int err = dstfs->mkdir(dir.path);
		if(err < 0) {
			return error(dstfs->getErrorString(err), "mkdir", dir.path);
		}
		if(dir.mtime != time) {
			dstfs->settime(dir.path, dir.mtime);
		}
		if(!copyFiles(dir.path)) {
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
		return error(hostfs.getErrorString(file), "open", srcFile);
	}
	Storage::FileDevice srcDevice("SRC", hostfs, file);
	auto part = srcDevice.createPartition("src", Storage::Partition::SubType::Data::fwfs, 0, hostfs.getSize(file),
										  Storage::Partition::Flag::readOnly);
	srcfs = IFS::createFirmwareFilesystem(part);
	srcfs->mount();

	// Destination
	file = hostfs.open(dstFile, File::CreateNewAlways | File::ReadWrite);
	if(file < 0) {
		return error(hostfs.getErrorString(file), "open", dstFile);
	}
	Storage::FileDevice dstDevice("DST", hostfs, file, dstSize);
	dstDevice.erase_range(0, dstSize);
	part = dstDevice.createPartition("dst", Storage::Partition::SubType::Data::littlefs, 0, dstSize);
	dstfs = IFS::createLfsFilesystem(part);
	dstfs->mount();

	//
	bool res = copyFiles(nullptr);

	IFS::FileSystem::Info srcinfo;
	srcfs->getinfo(srcinfo);
	IFS::FileSystem::Info dstinfo;
	dstfs->getinfo(dstinfo);
	IFS::FileSystem::PerfStat perfStat;
	dstfs->getPerfStat(perfStat);

	auto kb = [](size_t size) { return (size + 1023) / 1024; };

	m_printf("Source %s size: %u KB; Output %s used: %u KB, free: %u KB\r\n", toString(srcinfo.type).c_str(),
			 kb(srcinfo.used()), toString(dstinfo.type).c_str(), kb(dstinfo.used()), kb(dstinfo.freeSpace));
	m_printf("Perf stats: %s\r\n", perfStat.toString().c_str());

	return res;
}

}; // namespace

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
		auto res = fscopy(parameters[0].text, parameters[1].text, size);
		delete dstfs;
		delete srcfs;
		if(!res) {
			abort();
		}
	}
	System.restart();
}
