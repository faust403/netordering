project(net VERSION 1.1) 
 
set(CMAKE_CXX_STANDARD 20) 


add_executable(
	${PROJECT_NAME}
	"main.cpp"
) 

add_library(
		netordering
	STATIC
		"include/netordering/net.hpp"
		"src/net.cpp"
)


find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

IF(Boost_FOUND)
	target_link_libraries(
			${PROJECT_NAME}
		PRIVATE
			Boost::headers
			OpenSSL::SSL
			OpenSSL::Crypto
			netordering
	)
	target_link_libraries(
			netordering
		PRIVATE
			Boost::headers
	)
ENDIF()


target_include_directories(
	netordering
	PUBLIC
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		${Boost_INCLUDE_DIR}
)