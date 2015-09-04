# CORE_TODO: This build logic may have been better in the primary Linux CMakeList.txt however some dependany issue occurs when doing so
# related copying the kernel files for FQTSS. Basically the FQTSS custom commands for copy files was not engaged.

# Places to get include files
include_directories(
		${AVB_OSAL_DIR}/endpoint
		${AVB_OSAL_DIR}/maap
		${AVB_OSAL_DIR}/tl
		${AVB_SRC_DIR}/maap
		${AVB_SRC_DIR}/endpoint
		${AVB_SRC_DIR}/srp
		${AVB_SRC_DIR}/tl
		${AVB_OSAL_DIR}/fqtss/qmgr
		${AVB_OSAL_DIR}/srp
		)

SET ( SRC_FILES "" )
add_library (rawsock ${AVB_OSAL_DIR}/rawsock/openavb_rawsock.c)
add_subdirectory ( ${AVB_SRC_DIR}/maap ${CMAKE_BINARY_DIR}/maap )
add_subdirectory ( ${AVB_SRC_DIR}/srp ${CMAKE_BINARY_DIR}/srp )
add_subdirectory ( ${AVB_SRC_DIR}/endpoint ${CMAKE_BINARY_DIR}/endpoint )
add_library ( endpoint ${SRC_FILES} )
set ( AVB_ENDPOINT_LIBRARIES avbTl pthread rt m )
if ( AVB_FEATURE_FQTSS )
   # set define for ifdefs
   set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVB_FEATURE_FQTSS=1" )
   # and add library to link list
   set ( AVB_ENDPOINT_LIBRARIES "${AVB_ENDPOINT_LIBRARIES}" qmgr )
endif ( AVB_FEATURE_FQTSS )
target_link_libraries (endpoint ${AVB_ENDPOINT_LIBRARIES} )
add_executable (
	openavb_endpoint
	${AVB_OSAL_DIR}/endpoint/openavb_endpoint_osal.c
	)
target_link_libraries (openavb_endpoint endpoint )
install ( TARGETS openavb_endpoint RUNTIME DESTINATION ${AVB_INSTALL_BIN_DIR} )


