# add a target to generate API documentation with Doxygen
find_package(Doxygen)

if(DOXYGEN_FOUND)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
               ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/DoxygenLayout.xml
               ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml @ONLY)

add_custom_target(doc
	${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating API documentation with Doxygen" VERBATIM
)

add_custom_target(pdfdoc
	make 1>/dev/null 2>/dev/null
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/latex
	DEPENDS doc
	COMMENT "Generating PDF document"
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/api_docs
        DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR} OPTIONAL)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
        DESTINATION ${SDK_INSTALL_SDK_INTF_MOD_DIR}
        RENAME "OPENAVB EAVB SDK.pdf"
        OPTIONAL)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/api_docs
        DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR} OPTIONAL)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
        DESTINATION ${SDK_INSTALL_SDK_EAVB_DIR}
        RENAME "OPENAVB EAVB SDK.pdf"
        OPTIONAL)


endif(DOXYGEN_FOUND)
