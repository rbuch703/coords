CMAKE_MINIMUM_REQUIRED (VERSION 2.6)


PROJECT (coords)

include_directories (.)    
include_directories (src)

#find_package(Protobuf REQUIRED)
#PROTOBUF_GENERATE_CPP(fileformat.pb.cc fileformat.pb.h proto/fileformat.pb)

set(CMAKE_BUILD_TYPE Release)

ADD_EXECUTABLE(coordsCreateDatabase 
    src/conv_osm.cc 
    src/mem_map.cc 
    src/osm/osmTypes.cc 
    src/osm/osmMappedTypes.cc 
    src/osm/osm_tags.cc 
    src/osm/osmParserXml.cc
    src/osm/osmParserPbf.cc 
    src/consumers/osmConsumer.cc 
    src/consumers/osmConsumerCounter.cc
    src/consumers/osmConsumerDumper.cc 
    src/consumers/osmConsumerIdRemapper.cc
    src/containers/chunkedFile.cc
    proto/fileformat.pb.cc 
    proto/osmformat.pb.cc)


TARGET_LINK_LIBRARIES( coordsCreateDatabase -lprotobuf -lz)


ADD_CUSTOM_COMMAND( OUTPUT proto/fileformat.pb.cc proto/fileformat.pb.h 
                    COMMAND protoc --cpp_out=. proto/fileformat.proto
                    DEPENDS proto/fileformat.proto)

ADD_CUSTOM_COMMAND( OUTPUT proto/osmformat.pb.cc proto/osmformat.pb.h 
                    COMMAND protoc --cpp_out=. proto/osmformat.proto
                    DEPENDS proto/osmformat.proto)


SET( CXX_FLAGS "-std=c++11 -Wall -Wextra -s")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}" )


ADD_EXECUTABLE(coordsResolveLocations
               src/wayIntegrator.cc 
               src/mem_map.cc
               src/osm/osmTypes.cc 
               src/osm/osmMappedTypes.cc 
               src/containers/chunkedFile.cc
               src/containers/reverseIndex.cc 
               )

ADD_EXECUTABLE(coordsCreateTiles
               src/tiler.cc 
               src/tiles.cc 
               src/mem_map.cc
               src/osm/osmTypes.cc 
               src/osm/osmMappedTypes.cc 
               src/containers/chunkedFile.cc)
               
               

#EXECUTE_PROCESS(COMMAND mapnik-config --input-plugins 
#               OUTPUT_VARIABLE MAPNIK_PLUGIN_DIR 
#               OUTPUT_STRIP_TRAILING_WHITESPACE)


#TARGET_LINK_LIBRARIES( coords.input -lmapnik -licui18n -licuuc)


INSTALL (TARGETS coordsCreateDatabase DESTINATION /usr/bin )
INSTALL (TARGETS coordsResolveLocations DESTINATION /usr/bin )
INSTALL (TARGETS coordsCreateTiles DESTINATION /usr/bin )

#SET(CMAKE_BUILD_TYPE Release)
