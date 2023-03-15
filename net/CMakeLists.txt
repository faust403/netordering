project(net VERSION 1.1) 
 
set(CMAKE_CXX_STANDARD 20) 
 
SET(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "C:/Boost/include/boost-1_81") 
SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "C:/Boost/lib") 
 
find_package(Boost REQUIRED)  
IF(Boost_FOUND) 
    include_directories(${Boost_INCLUDE_DIR}) 
    link_directories(${Boost_LIBRARY_PATH}) 
ENDIF()

find_package(OpenSSL REQUIRED)
 
add_executable (${PROJECT_NAME} "main.cpp" "net.cpp" "net.hpp") 
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)