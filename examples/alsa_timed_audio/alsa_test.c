/******************************************************************************

  Copyright (c) 2018, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
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

#include <alsa.h>
#include <stdio.h>
#include <inttypes.h>

int main()
{
	struct isaudk_output *output;
	struct isaudk_format format;
	struct isaudk_cross_time cross_time;

	format.encoding = ISAUDK_ENC_PS16;
	format.channels = 2;

	// Open alsa
	output = isaudk_get_default_alsa_output();
	if( output == NULL )
		printf( "Get default ALSA output failed\n" );
	else
		printf( "Get default ALSA output succeeded\n" );

	if( !output->fn->set_audio_param( output->ctx, &format,
					  _48K_SAMPLE_RATE ) )
		printf( "Set ALSA output params failed\n" );
	else
		printf( "Set ALSA output params succeeded\n" );

	if( !output->fn->get_cross_tstamp( output->ctx, &cross_time ))
		printf( "Get ALSA cross timestamp failed\n" );
	else
		printf( "Get ALSA cross timestamp succeeded\n" );

	printf( "System time: %"PRIu64"\n", cross_time.sys.time );
	printf( "Audio time: %"PRIu64"\n", cross_time.dev.time );

	return 0;
}
