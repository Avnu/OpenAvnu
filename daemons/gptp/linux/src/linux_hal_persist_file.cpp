/*************************************************************************************************************
Copyright (c) 2012-2016, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************************************************/

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>


#include "linux_hal_persist_file.hpp"

class LinuxGPTPPersistFile : public GPTPPersist {
private:
	std::string persistIDStr;
	gPTPPersistWriteCB_t writeCB;

	int persistFD;
	void *restoredata;
	off_t storedDataLength;
	off_t memoryDataLength;

public:
	LinuxGPTPPersistFile() {
		persistFD = -1;
		restoredata = ((void *)-1);
		storedDataLength = 0;
		memoryDataLength = 0;
	}

	~LinuxGPTPPersistFile() {} ;

	bool initStorage(const char *persistID) {
		persistIDStr = persistID;

		persistFD = open(persistID, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (persistFD == -1) {
			printf("Failed to open restore file\n");
			return false;
		}
		return true;
	}

	bool closeStorage(void) {
		if (persistFD != -1) {
			if (restoredata != ((void *) -1))
				munmap(restoredata, storedDataLength);
			close(persistFD);
		}
		return true;
	}

	bool readStorage(char **bufPtr, uint32_t *bufSize) {
		bool result = false;
		if (persistFD != -1) {
			// MMAP file
			struct stat stat0;
			if (fstat(persistFD, &stat0) == -1) {
				printf("Failed to stat restore file, %s\n", strerror(errno));
				storedDataLength = 0;
			}
			else {
				storedDataLength = stat0.st_size;
				if (storedDataLength != 0) {
					if ((restoredata = mmap(NULL, storedDataLength, PROT_READ | PROT_WRITE, MAP_SHARED, persistFD, 0)) == ((void *)-1)) {
						printf("Failed to mmap restore file, %s\n", strerror(errno));
					}
					else {
						*bufSize = storedDataLength;
						*bufPtr = (char *)restoredata;
						result = true;
					}
				}
			}
		}

		return result;
	}

	void registerWriteCB(gPTPPersistWriteCB_t writeCB)
	{
		this->writeCB = writeCB;
	}

	void setWriteSize(uint32_t dataSize)
	{
		memoryDataLength = dataSize;
	}

	bool triggerWriteStorage(void)
	{
		if (!writeCB) {
			printf("Persistent write callback not registered\n");
		}

		bool result = false;
		if (memoryDataLength > storedDataLength) {
			ftruncate(persistFD, memoryDataLength);
			if (restoredata != ((void *)-1)) {
				restoredata = mremap(restoredata, storedDataLength, memoryDataLength, MREMAP_MAYMOVE);
			}
			else {
				restoredata = mmap(NULL, memoryDataLength, PROT_READ | PROT_WRITE, MAP_SHARED, persistFD, 0);
			}
			if (restoredata == ((void *)-1)) {

			}
			else {
				storedDataLength = memoryDataLength;
				result = true;
			}
		}

		writeCB((char *)restoredata, storedDataLength);
		return result;
	}
};



GPTPPersist* makeLinuxGPTPPersistFile() {
	return new LinuxGPTPPersistFile();
}

