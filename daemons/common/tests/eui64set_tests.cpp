/****************************************************************************
  Copyright (c) 2014, J.D. Koftinoff Software, Ltd.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of J.D. Koftinoff Software, Ltd. nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <stdio.h>
#include <string.h>

#include "CppUTest/TestHarness.h"

extern "C" {

#include "eui64set.h"

} TEST_GROUP(Eui64SetGroup)
{
	void setup() {
	}

	void teardown() {
	}
};

TEST(Eui64SetGroup, Fill)
{
	eui64set my_set;
	int size = 7;
	CHECK(eui64set_init(&my_set, size) == 0);
	CHECK(eui64set_is_full(&my_set) == 0);

	for (int i = 0; i < size; ++i) {
		uint64_t v = (i - (uint64_t(~0)));
		CHECK(eui64set_insert_and_sort(&my_set, v, 0) == 1);
		if (i < size - 1) {
			CHECK(eui64set_is_full(&my_set) == 0);
		}
	}
	CHECK(eui64set_is_full(&my_set) == 1);
	eui64set_free(&my_set);
}

TEST(Eui64SetGroup, Find)
{
	eui64set my_set;
	int size = 7;
	CHECK(eui64set_init(&my_set, size) == 0);
	CHECK(eui64set_is_full(&my_set) == 0);

	for (int i = 0; i < size; ++i) {
		uint64_t v = (i - (uint64_t(~0)));
		CHECK(eui64set_insert_and_sort(&my_set, v, 0) == 1);
		if (i < size - 1) {
			CHECK(eui64set_is_full(&my_set) == 0);
		}
	}
	CHECK(eui64set_is_full(&my_set) == 1);

	for (int i = 0; i < size; ++i) {
		uint64_t v = (i - (uint64_t(~0)));

		const eui64set_entry *entry = eui64set_find(&my_set, v);
		CHECK(entry != 0);
		if (entry) {
			CHECK(entry->eui64 == v);
		}
	}

	CHECK(eui64set_find(&my_set, 0) == 0);

	eui64set_free(&my_set);

}

TEST(Eui64SetGroup, Remove)
{

}
