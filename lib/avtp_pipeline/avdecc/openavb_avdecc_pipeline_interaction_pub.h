/*
 ******************************************************************
 * COPYRIGHT (C) Harman International
 * All Rights Reserved
 ******************************************************************
 */

#ifndef OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H
#define OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H 1

#include "openavb_acmp.h"
#include "openavb_descriptor_stream_io_pub.h"


// Run a single talker or listener.
bool openavbAVDECCRunListener(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx, openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo);
bool openavbAVDECCRunTalker(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo);

// Stop a single talker or listener.
bool openavbAVDECCStopListener(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx, openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo);
bool openavbAVDECCStopTalker(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo);

// Get talker stream details. Structure members in TalkerStrreamInfo will be filled.
bool openavbAVDECCGetTalkerStreamInfo(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo);

// Determine if the talker or listener is streaming.
bool openavbAVDECCListenerIsStreaming(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx);
bool openavbAVDECCTalkerIsStreaming(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx);

// Pause or resume the stream.
void openavbAVDECCPauseStream(openavb_aem_descriptor_stream_io_t *pDescriptor, bool bPause);

// Get the current counter value in pValue.  Returns TRUE if the counter is supported, FALSE otherwise.
bool openavbAVDECCGetCounterValue(void *pDescriptor, U16 descriptorType, U32 counterFlag, U32 *pValue);

#endif // OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H
