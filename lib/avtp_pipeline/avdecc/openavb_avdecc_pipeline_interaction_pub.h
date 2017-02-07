/*
 ******************************************************************
 * COPYRIGHT (C) Harman International
 * All Rights Reserved
 ******************************************************************
 */

#ifndef OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H
#define OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H 1

#include "openavb_descriptor_stream_io_pub.h"


// Run a single talker or listener. At this point data can be sent or received. Used in place of the public openavbTLRun
bool openavbAVDECCRunListener(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, U16 descriptorType, U16 descriptorIdx, void *pVoidListenerStreamInfo);
bool openavbAVDECCRunTalker(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, U16 descriptorType, U16 descriptorIdx, void *pVoidTalkerStreamInfo);

// Stop a single talker or listener. At this point data will not be sent or received. Used in place of the public openavbTLStop.
bool openavbAVDECCStopListener(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, void *pVoidListenerStreamInfo);
bool openavbAVDECCStopTalker(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, void *pVoidTalkerStreamInfo);

// Get talker stream details. Structure members in TalkerStrreamInfo will be filled.
bool openavbAVDECCGetTalkerStreamInfo(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 configIdx, void *pVoidTalkerStreamInfo);

// Determine if the talker or listener is streaming.
bool openavbAVDECCIsStreaming(openavb_aem_descriptor_stream_io_t *pDescriptor);

// Pause or resume the stream.
void openavbAVDECCPauseStream(openavb_aem_descriptor_stream_io_t *pDescriptor, bool bPause);

// Get the current counter value in pValue.  Returns TRUE if the counter is supported, FALSE otherwise.
bool openavbAVDECCGetCounterValue(void *pDescriptor, U16 descriptorType, U32 counterFlag, U32 *pValue);

#endif // OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H
