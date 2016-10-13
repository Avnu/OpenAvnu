#include <stdio.h>
#include <string.h>

#include "CppUTest/TestHarness.h"

extern "C" {

#include "maap.h"

} TEST_GROUP(maap_group)
{
	void setup() {
	}

	void teardown() {
	}
};

TEST(maap_group, Fill)
{
	CHECK(0 == 0);
}

