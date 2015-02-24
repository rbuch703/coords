CMAKE_MINIMUM_REQUIRED (VERSION 2.6)


PROJECT (coords)

include(FindProtobuf)
find_package(Protobuf REQUIRED)

include_directories (.)
include_directories (src)

set(CMAKE_BUILD_TYPE RELWITHDEBINFO) #Debug)


PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders proto/osmformat.proto proto/fileformat.proto)

ADD_EXECUTABLE(coordsCreateStorage 
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
    ${ProtoSources} ${ProtoHeaders})


TARGET_LINK_LIBRARIES( coordsCreateStorage -lprotobuf -lz )

SET( CXX_FLAGS "-std=c++11 -Wall -Wextra")
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


INSTALL (TARGETS coordsCreateStorage DESTINATION /usr/bin )
INSTALL (TARGETS coordsResolveLocations DESTINATION /usr/bin )
INSTALL (TARGETS coordsCreateTiles DESTINATION /usr/bin )

