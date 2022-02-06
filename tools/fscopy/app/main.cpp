#include <SmingCore.h>
#include <LittleFS.h>
#include <Storage/FileDevice.h>
#include <IFS/FileCopier.h>
#include <hostlib/CommandLine.h>

namespace
{
bool fscopy(const char* srcFile, const char* dstFile, size_t dstSize)
{
	auto& hostfs = IFS::Host::getFileSystem();

	// Source
	auto srcfs = IFS::mountArchive(hostfs, srcFile);
	if(srcfs == nullptr) {
		m_printf("mount failed: %s", srcFile);
		return false;
	}

	// Destination
	auto file = hostfs.open(dstFile, File::CreateNewAlways | File::ReadWrite);
	if(file < 0) {
		debug_e("Error opening '%s', %s", dstFile.c_str(), hostfs.getErrorString(file).c_str());
	}
	Storage::FileDevice dstDevice("DST", hostfs, file, dstSize);
	dstDevice.erase_range(0, dstSize);
	auto part = dstDevice.createPartition("dst", Storage::Partition::SubType::Data::littlefs, 0, dstSize);
	auto dstfs = IFS::createLfsFilesystem(part);
	dstfs->mount();

	IFS::Profiler profiler;
	dstfs->setProfiler(&profiler);

	IFS::FileCopier copier(*srcfs, *dstfs);
	bool res = copier.copyDir(nullptr, nullptr);
	dstfs->setProfiler(nullptr);

	IFS::FileSystem::Info srcinfo;
	srcfs->getinfo(srcinfo);
	IFS::FileSystem::Info dstinfo;
	dstfs->getinfo(dstinfo);

	delete dstfs;
	delete srcfs;

	auto kb = [](size_t size) { return (size + 1023) / 1024; };

	m_printf("Source %s size: %u KB; Output %s used: %u KB, free: %u KB\r\n", toString(srcinfo.type).c_str(),
			 kb(srcinfo.used()), toString(dstinfo.type).c_str(), kb(dstinfo.used()), kb(dstinfo.freeSpace));
	m_printf("Perf stats: %s\r\n", profiler.toString().c_str());

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
		if(!res) {
			exit(2);
		}
	}

	exit(0);
}
