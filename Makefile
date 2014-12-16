
CONV_OSM_SRC = conv_osm.cc mem_map.cc osm_types.cc osm_tags.cc osmParserXml.cc\
               osmParserPbf.cc osmConsumer.cc osmConsumerCounter.cc osmConsumerDumper.cc osmConsumerIdRemapper.cc
#PB_TEST_SRC = osmParserPbf.cc osm_types.cc mem_map.cc
#REMAP_SRC = remapper.cc mem_map.cc osm_types.cc osm_tags.cc osmxmlparser.cc idRemappingParser.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc
SIMP_SRC = simplifier.cc osm_types.cc mem_map.cc

PROTO_DEF = proto/fileformat.proto proto/osmformat.proto
PROTO_SRC = $(patsubst %.proto,%.pb.cc,$(PROTO_DEF))
PROTO_OBJ = $(patsubst %.cc,%.o,$(PROTO_SRC))

CONV_OSM_OBJ  = $(patsubst %.cc,build/%.o,$(CONV_OSM_SRC)) $(PROTO_OBJ)
#PB_TEST_OBJ = $(patsubst %.cc,build/%.o,$(PB_TEST_SRC)) $(PROTO_OBJ)
#REMAP_OBJ  = $(REMAP_SRC:.cc=.o)
CONV_OBJ = $(patsubst %.cc,build/%.o,$(CONV_SRC))
SIMP_OBJ = $(patsubst %.cc,build/%.o,$(SIMP_SRC))

FLAGS = -g -Wall -Wextra #-DNDEBUG -O2 -flto
#FLAGS = -ftrapv -g -Wall -Wextra 
#FLAGS = -ftrapv -g -Wall -Wextra -fprofile-arcs -ftest-coverage
CFLAGS = $(FLAGS) -std=c99
CCFLAGS = $(FLAGS) -std=c++11
LD_FLAGS = #-flto -O2 #-fprofile-arcs#--as-needed
.PHONY: all clean
.SECONDARY: $(PROTO_SRC)
all: build make.dep build/conv_osm build/simplifier build/data_converter intermediate 
#	 @echo [ALL] $<

build:
	@echo [MKDIR ] $@
	@mkdir -p build

make_index: make_index.cc
	@echo [LD ] $@
	@g++ $^ -o $@

intermediate: 
	@echo [MKDIR ] $@
	@mkdir $@

remap: $(REMAP_OBJ)
	@echo [LD ] $@
	@g++ $^  $(LD_FLAGS) -o $@

build/conv_osm: $(CONV_OSM_OBJ)
	@echo [LD ] $@
	@g++ $^ $(LD_FLAGS) -lprotobuf -lz -o $@

build/data_converter: $(CONV_OBJ)
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(LD_FLAGS) $^ GeoLib/build/geolib.a -lgmp -lgmpxx -o $@

build/simplifier: $(SIMP_OBJ)
	@echo [LD ] $@
	@g++ $^ $(CCFLAGS) $(LD_FLAGS) GeoLib/build/geolib.a -lgmp -lgmpxx -o $@

build/data_converter.o: data_converter.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) -IGeoLib $< -c -o $@

build/simplifier.o: simplifier.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) -IGeoLib $< -c -o $@


build/%.o: %.cc 
	@echo [C++] $<
	@g++ $(CCFLAGS) $< -c -o $@

proto/%.o: proto/%.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) -I. $< -c -o $@

proto/%.pb.cc proto/%.pb.h: proto/%.proto
	@echo [PBC] $<
	@protoc --cpp_out=. $<

clean:
	@echo [CLEAN]
	@rm -rf *~
	@rm -rf *gcda
	@rm -rf *gcno
#	@rm -rf conv_osm data_converter simplifier make_index remap
	@rm -rf coverage.info callgrind.out.*
	@rm -rf build/*
	@rm -rf proto/*.o proto/*.pb.h proto/*.pb.cc

make.dep: $(CONV_OSM_SRC) $(CONV_SRC) $(SIMP_SRC)
	@echo [DEP]
	@g++ -MM -MG $^ | sed "s/\([[:graph:]]*\)\.o/build\/\\1.o/g" > make.dep

include make.dep

