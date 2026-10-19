# Fork addition: Discord Rich Presence module, included at the end of gui/CMakeLists.txt.

find_package(Qt6 REQUIRED COMPONENTS Network)

target_sources(chiaki PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/discordpresence.h
	${CMAKE_CURRENT_LIST_DIR}/discordipc.h
	${CMAKE_CURRENT_LIST_DIR}/discordipc.cpp
	${CMAKE_CURRENT_LIST_DIR}/presenceactivity.h
	${CMAKE_CURRENT_LIST_DIR}/presenceactivity.cpp
	${CMAKE_CURRENT_LIST_DIR}/presencecontroller.h
	${CMAKE_CURRENT_LIST_DIR}/presencecontroller.cpp
	${CMAKE_CURRENT_LIST_DIR}/presencelog.h
	${CMAKE_CURRENT_LIST_DIR}/presencevalidation.h
	${CMAKE_CURRENT_LIST_DIR}/titlemetadata.h
	${CMAKE_CURRENT_LIST_DIR}/titlemetadata.cpp
	${CMAKE_CURRENT_LIST_DIR}/titlesource.h
	${CMAKE_CURRENT_LIST_DIR}/titlesource.cpp
)
target_include_directories(chiaki PRIVATE ${CMAKE_CURRENT_LIST_DIR})
# plain signature: gui/CMakeLists.txt links chiaki without keywords and CMake forbids mixing
target_link_libraries(chiaki Qt::Network)
