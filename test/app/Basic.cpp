#include <SmingTest.h>
#include <IFS/Helpers.h>
#include <IFS/FWFS/ArchiveStream.h>
#include <Storage/FileDevice.h>
#include <LittleFS.h>

class BasicTest : public TestGroup
{
public:
	BasicTest() : TestGroup(_F("Basic"))
	{
	}

	void execute() override
	{
	}
};

void REGISTER_TEST(basic)
{
	registerGroup<BasicTest>();
}
