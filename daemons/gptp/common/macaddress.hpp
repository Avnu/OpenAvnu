/*
 * Copyright (C) 2017 Art and Logic.
 *
 * A mac address abstration providing a minimal interface.
 *
 */

#pragma once

#include <net/if.h>

class AMacAddress
{
	public:
		AMacAddress()
		{
			memset(fOctets, 0, kMaxOctets);
		}
		
		AMacAddress(const ifreq& device)
		{
			const uint8_t *sourceMacAddress = reinterpret_cast<const uint8_t *>(
			 &device.ifr_hwaddr.sa_data);
			const uint8_t *kOctetEnd = fOctets + kMaxOctets;
			const uint8_t *kSourceMacEnd = sourceMacAddress + kMaxOctets;
			for (uint8_t *begin = fOctets;
			 begin < kOctetEnd && sourceMacAddress < kSourceMacEnd;
			 ++begin, ++sourceMacAddress)
			{
				*begin = *sourceMacAddress;
			}
		}

		AMacAddress(const AMacAddress& other)
		{
			memcpy(fOctets, other.fOctets, kMaxOctets);
		}

		AMacAddress& operator =(const AMacAddress& other)
		{
			if (this != &other)
			{
				memcpy(fOctets, other.fOctets, kMaxOctets);
			}
			return *this;
		}

		void Copy(uint8_t* dest, size_t size) const
		{
			if (dest != nullptr)
			{
				memcpy(dest, fOctets, size <= kMaxOctets ? size : kMaxOctets);
			}
		}

	private:
		static const int kMaxOctets = 14;
		uint8_t fOctets[kMaxOctets];
};
