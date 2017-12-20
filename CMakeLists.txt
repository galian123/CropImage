cmake_minimum_required(VERSION 2.8)
project( CropImage )
find_package( OpenCV REQUIRED )
add_executable( CropImage CropImage.cpp )

set(CMAKE_CXX_FLAGS "-g")

include_directories ( 
    /usr/local/include  
    /usr/include 
    ${OpenCV_INCLUDE_DIRS} )  

target_link_libraries( CropImage 
    /usr/lib/x86_64-linux-gnu/libglog.so
    /usr/lib/x86_64-linux-gnu/libgflags.so
    /usr/lib/x86_64-linux-gnu/libboost_system.so
    ${OpenCV_LIBS} )

INSTALL(TARGETS CropImage
    DESTINATION $ENV{HOME}/bin)
