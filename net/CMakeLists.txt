project(net VERSION 1.1) 
 
set(CMAKE_CXX_STANDARD 20) 

find_package(Boost REQUIRED)  
IF(Boost_FOUND) 
    include_directories(${Boost_INCLUDE_DIR}) 
    link_directories(${Boost_LIBRARY_PATH}) 
ENDIF()

find_package(OpenSSL REQUIRED)

add_library(netordering STATIC "include/netordering/net.hpp" "src/net.cpp")
target_include_directories(
	netordering
	PUBLIC
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		${Boost_INCLUDE_DIR}
)

target_link_libraries(
	netordering
	PRIVATE
		${Boost_LIBRARIES}
)

add_executable (${PROJECT_NAME} "main.cpp") 
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto netordering)