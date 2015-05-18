CMAKE_MINIMUM_REQUIRED (VERSION 2.6)


PROJECT (coords)

include(FindProtobuf)
find_package(Protobuf REQUIRED)

include_directories (.)
include_directories (src)
include_directories( ${CMAKE_BINARY_DIR} )

#set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
set(CMAKE_BUILD_TYPE Debug)


PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders proto/osmformat.proto proto/fileformat.proto)

ADD_EXECUTABLE(coordsCreateStorage 
    src/conv_osm.cc 
    src/misc/mem_map.cc 
    src/osm/osmTypes.cc 
#    src/osm/osmMappedTypes.cc 
    src/osm/osmBaseTypes.cc
    src/osm/osm_tags.cc 
    src/osm/osmParserXml.cc
    src/osm/osmParserPbf.cc 
    src/geom/envelope.cc
    src/consumers/osmConsumer.cc 
    src/consumers/osmConsumerCounter.cc
    src/consumers/osmConsumerDumper.cc 
    src/consumers/osmConsumerIdRemapper.cc
    src/containers/chunkedFile.cc
    ${ProtoSources} ${ProtoHeaders})


TARGET_LINK_LIBRARIES( coordsCreateStorage -lprotobuf -lz )

SET( CXX_FLAGS "-std=c++11 -Wall -Wextra")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}" )


ADD_EXECUTABLE(coordsResolveStorage
               src/resolveRefs.cc 
               src/misc/mem_map.cc
               src/osm/osmTypes.cc 
               src/osm/osmBaseTypes.cc 
               src/osm/osmMappedTypes.cc 
               src/geom/envelope.cc
               src/containers/chunkedFile.cc
               src/containers/reverseIndex.cc 
               src/containers/osmWayStore.cc
               src/containers/osmRelationStore.cc
               )

ADD_EXECUTABLE(coordsCreateTiles
               src/tiler.cc 
               src/tiles.cc 
               src/misc/mem_map.cc
               src/osm/osmTypes.cc 
               src/osm/osmMappedTypes.cc 
               src/osm/osmBaseTypes.cc
               src/geom/envelope.cc
               src/geom/genericGeometry.cc
               src/geom/ring.cc
               src/geom/geomSerializers.cc
               src/containers/chunkedFile.cc
               src/containers/osmWayStore.cc
               src/misc/symbolicNames.cc
               src/misc/rawTags.cc
               )

ADD_EXECUTABLE(multipolygonReconstructor
               src/multipolygonReconstructor.cc
               src/geom/ringSegment.cc
               src/geom/ring.cc
               src/geom/ringAssembler.cc
               src/geom/envelope.cc
               src/geom/genericGeometry.cc
               src/geom/geomSerializers.cc
               src/osm/osmBaseTypes.cc
               src/osm/osmTypes.cc 
               src/osm/osmMappedTypes.cc
               src/misc/mem_map.cc
               src/containers/chunkedFile.cc               
               src/containers/osmRelationStore.cc
               )

ADD_EXECUTABLE(geomReader
               src/geomReader.cc
               src/geom/envelope.cc
               src/geom/genericGeometry.cc
#               src/geom/ring.cc
#               src/mem_map.cc
#               src/osm/osmTypes.cc 
#               src/osm/osmBaseTypes.cc 
#               src/osm/osmMappedTypes.cc
#               src/containers/chunkedFile.cc               
               )

ADD_EXECUTABLE(unmapId
               src/unmapId.cc
               )


TARGET_LINK_LIBRARIES( geomReader -lgeos )
TARGET_LINK_LIBRARIES( coordsCreateTiles -lgeos)



TARGET_LINK_LIBRARIES( multipolygonReconstructor -lgeos )


ADD_CUSTOM_TARGET(man ALL)

ADD_CUSTOM_COMMAND(
  TARGET man
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/man/coordsCreateStorage.1.ronn
  COMMAND ronn -r < ${CMAKE_CURRENT_SOURCE_DIR}/man/coordsCreateStorage.1.ronn > ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateStorage.1
  OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateStorage.1
)

ADD_CUSTOM_COMMAND(
  TARGET man
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/man/coordsResolveStorage.1.ronn
  COMMAND ronn -r < ${CMAKE_CURRENT_SOURCE_DIR}/man/coordsResolveStorage.1.ronn > ${CMAKE_CURRENT_BINARY_DIR}/coordsResolveStorage.1
  OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/coordsResolveStorage.1
)

ADD_CUSTOM_COMMAND(
  TARGET man
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/man/coordsCreateTiles.1.ronn
  COMMAND ronn -r < ${CMAKE_CURRENT_SOURCE_DIR}/man/coordsCreateTiles.1.ronn > ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateTiles.1
  OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateTiles.1
)


ADD_CUSTOM_COMMAND(
  TARGET man
  SOURCE man
  DEPENDS 
    ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateStorage.1
    ${CMAKE_CURRENT_BINARY_DIR}/coordsResolveStorage.1
    ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateTiles.1
)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateStorage.1    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/coordsResolveStorage.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/coordsCreateTiles.1      DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)

INSTALL (TARGETS coordsCreateStorage DESTINATION /usr/bin )
INSTALL (TARGETS coordsResolveStorage DESTINATION /usr/bin )
INSTALL (TARGETS coordsCreateTiles DESTINATION /usr/bin )

