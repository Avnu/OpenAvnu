/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Entity Model
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the 1722.1 (AVDECC) Entity Model
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#include "stdlib.h"

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_avdecc.h"
#include "openavb_aem.h"

MUTEX_HANDLE(openavbAemMutex);
#define AEM_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(openavbAemMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define AEM_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(openavbAemMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

static openavb_avdecc_entity_model_t *pAemEntityModel = NULL;

////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemCreate(openavb_aem_descriptor_entity_t *pDescriptorEntity)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	MUTEX_ATTR_HANDLE(mta);
	MUTEX_ATTR_INIT(mta);
	MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
	MUTEX_ATTR_SET_NAME(mta, "openavbAemMutex");
	MUTEX_CREATE_ERR();
	MUTEX_CREATE(openavbAemMutex, mta);
	MUTEX_LOG_ERR("Could not create/initialize 'openavbAemMutex' mutex");

	if (!pDescriptorEntity) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_AEM);
	}

	if (!pAemEntityModel) {
		pAemEntityModel = calloc(1, sizeof(*pAemEntityModel));
	}

	if (!pAemEntityModel) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY), AVB_TRACE_AEM);
	}

	pAemEntityModel->pDescriptorEntity = pDescriptorEntity;
	pAemEntityModel->aemConfigurations = openavbArrayNewArray(sizeof(openavb_aem_configuration_t));

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDestroy()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	// AVDECC_TODO all deallocations will occur here

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

bool openavbAemCheckModel(bool bLog)
{
	if (!pAemEntityModel || !pAemEntityModel->pDescriptorEntity) {
		if (bLog) {
			AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING);
		}
		return FALSE;
	}
	return TRUE;
}

openavbRC openavbAemAddDescriptorToConfiguration(U16 descriptorType, U16 configIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	int i1;

	// Make sure Entity Model is created
	if (!openavbAemCheckModel(FALSE)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_AEM);
	}

	openavb_aem_configuration_t *pConfiguration = openavbArrayDataIdx(pAemEntityModel->aemConfigurations, configIdx);
	if (!pConfiguration) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_INVALID_CONFIG_IDX), AVB_TRACE_AEM);
	}

	openavb_aem_descriptor_configuration_t *pConfig = pConfiguration->pDescriptorConfiguration;
	if (!pConfig) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_INVALID_CONFIG_IDX), AVB_TRACE_AEM);
	}

	// Check if the new descriptor type is in the configuration array
	for (i1 = 0; i1 < pConfig->descriptor_counts_count; i1++) {
		if (pConfig->descriptor_counts[i1].descriptor_type == descriptorType) {
			// Found this descritor type so just increase the count.
			pConfig->descriptor_counts[i1].count++; 
			AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
		}
	}

	// Update counts
	pConfig->descriptor_counts[pConfig->descriptor_counts_count].descriptor_type = descriptorType;
	pConfig->descriptor_counts[pConfig->descriptor_counts_count].count++; 
	pConfig->descriptor_counts_count++;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavb_avdecc_entity_model_t *openavbAemGetModel()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pAemEntityModel;
}

openavb_array_t openavbAemGetDescriptorArray(U16 configIdx, U16 descriptorType)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_array_t retDescriptors = NULL;

	// Make sure Entity Model is created
	if (!openavbAemCheckModel(TRUE)) {
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	if (descriptorType == OPENAVB_AEM_DESCRIPTOR_ENTITY) {
		// There is always only a single Entity Descriptor and therefore there isn't an array. NULL will be returned.
	}
	else if (descriptorType == OPENAVB_AEM_DESCRIPTOR_CONFIGURATION) {
		// The configuration descriptor is a special descriptor and not return by this function. NULL will be returned.
	}
	else {
		openavb_aem_configuration_t *pConfig = openavbArrayDataIdx(pAemEntityModel->aemConfigurations, configIdx);
		if (pConfig) {
			retDescriptors = pConfig->descriptorsArray[descriptorType];
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return retDescriptors;
}

void *openavbAemFindDescriptor(U16 configIdx, U16 descriptorType, U16 descriptorIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	void *retDescriptor = NULL;

	// Make sure Entity Model is created
	if (!openavbAemCheckModel(TRUE)) {
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	if (descriptorType == OPENAVB_AEM_DESCRIPTOR_ENTITY) {
		retDescriptor = pAemEntityModel->pDescriptorEntity;
	}
	else if (descriptorType == OPENAVB_AEM_DESCRIPTOR_CONFIGURATION) {
		openavb_aem_configuration_t *pConfig = openavbArrayDataIdx(pAemEntityModel->aemConfigurations, configIdx);
		if (pConfig) {
			retDescriptor = pConfig->pDescriptorConfiguration;
		}
	}
	else {
		openavb_array_t descriptors = NULL;
		openavb_aem_configuration_t *pConfig = openavbArrayDataIdx(pAemEntityModel->aemConfigurations, configIdx);
		if (pConfig) {
			descriptors = pConfig->descriptorsArray[descriptorType];
		}

		if (descriptors) {
			retDescriptor = openavbArrayDataIdx(descriptors, descriptorIdx);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return retDescriptor;
}

openavbRC openavbAemSerializeDescriptor(U16 configIdx, U16 descriptorType, U16 descriptorIdx, U16 bufSize, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavbRC retCode = OPENAVB_AVDECC_SUCCESS;

	// Make sure Entity Model is created
	if (!openavbAemCheckModel(FALSE)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING), AVB_TRACE_AEM);
	}

	if (!pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	void *pDescriptor = openavbAemFindDescriptor(configIdx, descriptorType, descriptorIdx);
	if (pDescriptor) {
		openavb_aem_descriptor_common_t *pDescriptorCommon = pDescriptor;
		pDescriptorCommon->descriptorPvtPtr->toBuf(pDescriptor, bufSize, pBuf, descriptorSize);
	}

	AVB_RC_TRACE_RET(retCode, AVB_TRACE_AEM);
}

bool openavbAemAddDescriptorConfiguration(openavb_aem_descriptor_configuration_t *pDescriptor, U16 *pResultIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	// Make sure Entity Model is created
	if (!openavbAemCheckModel(TRUE)) {
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	if (!pDescriptor || !pResultIdx) {
		AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT);
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	// Allocate the new configuration structure
	openavb_aem_configuration_t *pAemConfiguration = openavbArrayDataNew(pAemEntityModel->aemConfigurations);
	if (!pAemConfiguration) {
		AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY);
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	// Assign the descriptor to the new configuration
	pAemConfiguration->pDescriptorConfiguration = pDescriptor;

	// Set to return descriptor index and increment the count
	S32 retIdx = openavbArrayFindData(pAemEntityModel->aemConfigurations, pAemConfiguration);
	if (retIdx  < 0) {
		return FALSE;
	}
	*pResultIdx = retIdx;

	pAemEntityModel->pDescriptorEntity->configurations_count++;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT U16 openavbAemGetConfigIdx()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	// This function does NOT check for errors but at a minimum avoid a crash.
	if (!pAemEntityModel || !pAemEntityModel->pDescriptorEntity) {
		AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING);
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return 0;
	}
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pAemEntityModel->pDescriptorEntity->current_configuration;
}


extern DLL_EXPORT bool openavbAemAddDescriptor(void *pDescriptor, U16 configIdx, U16 *pResultIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	// Make sure Entity Model is created
	if (!openavbAemCheckModel(TRUE)) {
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	if (!pDescriptor || !pResultIdx) {
		AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT);
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	openavb_aem_descriptor_common_t *pDescriptorCommon = pDescriptor;

	if (pDescriptorCommon->descriptor_type == OPENAVB_AEM_DESCRIPTOR_ENTITY) {
		// Entity Descriptors aren't handled in this function.
		AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT);
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}
	if (pDescriptorCommon->descriptor_type == OPENAVB_AEM_DESCRIPTOR_CONFIGURATION) {
		// Configuration descriptors have special processing
		bool result = openavbAemAddDescriptorConfiguration(pDescriptor, pResultIdx);
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return result;
	}

	openavb_array_t descriptors = NULL;
	openavb_aem_configuration_t *pConfig = openavbArrayDataIdx(pAemEntityModel->aemConfigurations, configIdx);
	if (pConfig) {
		descriptors = pConfig->descriptorsArray[pDescriptorCommon->descriptor_type];
	}

	if (!descriptors) {
		// Create array for this type of descriptor in this configuration
		descriptors = openavbArrayNewArray(pDescriptorCommon->descriptorPvtPtr->size);
		pConfig->descriptorsArray[pDescriptorCommon->descriptor_type] = descriptors;
	}

	if (descriptors) {
		openavb_array_elem_t elem = openavbArrayAdd(descriptors, pDescriptor);
		if (elem) {
			*pResultIdx = openavbArrayGetIdx(elem);
			pDescriptorCommon->descriptor_index = *pResultIdx;
			if (pDescriptorCommon->descriptorPvtPtr->bTopLevel) {
				if (IS_OPENAVB_FAILURE(openavbAemAddDescriptorToConfiguration(pDescriptorCommon->descriptor_type, configIdx))) {
					AVB_TRACE_EXIT(AVB_TRACE_AEM);
					return TRUE;
				}
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return FALSE;
}

extern DLL_EXPORT void *openavbAemGetDescriptor(U16 configIdx, U16 descriptorType, U16 descriptorIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	void *retDescriptor = NULL;

	retDescriptor = openavbAemFindDescriptor(configIdx, descriptorType, descriptorIdx);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return retDescriptor;
}

extern DLL_EXPORT bool openavbAemSetString(U8 *pMem, const char *pString)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (pMem && pString) {
		U32 len = strlen(pString);
		if (len > OPENAVB_AEM_STRLEN_MAX) {
			len = OPENAVB_AEM_STRLEN_MAX;
		} else if (len < OPENAVB_AEM_STRLEN_MAX) {
			memset(pMem + len, 0, OPENAVB_AEM_STRLEN_MAX - len);
		}
		memcpy(pMem, pString, len);		// Per 1722.1 it is allowable that the AEM string may not be null terminated.
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return TRUE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return FALSE;
}

extern DLL_EXPORT bool openavbAemIsAcquired()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	AEM_LOCK();
	bool bResult = pAemEntityModel->entityAcquired;
	AEM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return bResult;
}

extern DLL_EXPORT bool openavbAemIsLocked()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	AEM_LOCK();
	bool bResult = pAemEntityModel->entityLocked;
	AEM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return bResult;
}

extern DLL_EXPORT bool openavbAemAcquire(bool flag)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	AEM_LOCK();
	bool bResult = FALSE;
	if (flag) {
		if (!pAemEntityModel->entityAcquired) {
			*(pAemEntityModel->acquiredControllerId) = (U64)0x0000000000000000L;
			pAemEntityModel->entityAcquired = TRUE;
			bResult = TRUE;
		}
	}
	else {
		if (pAemEntityModel->entityAcquired) {
			if ((U64)*pAemEntityModel->acquiredControllerId == 0x0000000000000000L) {
				// Only unacquire if controller ID is zero. 
				pAemEntityModel->entityAcquired = FALSE;
				bResult = TRUE;
			}
		}
	}
	AEM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return bResult;
}

extern DLL_EXPORT bool openavbAemLock(bool flag)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	AEM_LOCK();
	bool bResult = FALSE;
	if (flag) {
		if (!pAemEntityModel->entityLocked) {
			*(pAemEntityModel->lockedControllerId) = (U64)0x0000000000000000L;
			pAemEntityModel->entityLocked = TRUE;
			bResult = TRUE;
		}
	}
	else {
		if (pAemEntityModel->entityLocked) {
			if ((U64)*pAemEntityModel->lockedControllerId == 0x0000000000000000L) {
				// Only unlock if controller ID is zero. 
				pAemEntityModel->entityLocked = FALSE;
				bResult = TRUE;
			}
		}
	}
	AEM_UNLOCK();
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return bResult;
}

