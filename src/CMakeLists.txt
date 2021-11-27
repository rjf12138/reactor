project(reactor)

target_include_directories(reactor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(reactor PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src/./client.cc
		${CMAKE_CURRENT_SOURCE_DIR}/src/./reactor.cc
		${CMAKE_CURRENT_SOURCE_DIR}/src/./server.cc)
