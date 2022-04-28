project(reactor)

target_include_directories(reactor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/reactor)

target_sources(reactor PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/reactor/./linux_reactor.cc)
