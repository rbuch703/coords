
CONV_OSM_SRC = conv_osm.cc mem_map.cc osmTypes.cc osmMappedTypes.cc osm_tags.cc osmParserXml.cc\
               osmParserPbf.cc osmConsumer.cc osmConsumerCounter.cc osmConsumerDumper.cc \
               chunkedFile.cc
WAYINT_SRC = wayIntegrator.cc reverseIndex.cc mem_map.cc osmTypes.cc osmMappedTypes.cc chunkedFile.cc
TILER_SRC = tiler.cc tiles.cc osmTypes.cc osmMappedTypes.cc mem_map.cc chunkedFile.cc

#CONV_SRC = data_converter.cc osmTypes.cc mem_map.cc
#SIMP_SRC = simplifier.cc osmTypes.cc mem_map.cc

PROTO_DEF = proto/fileformat.proto proto/osmformat.proto
PROTO_SRC = $(patsubst %.proto,%.pb.cc,$(PROTO_DEF))
PROTO_OBJ = $(patsubst %.cc,%.o,$(PROTO_SRC))

CONV_OSM_OBJ  = $(patsubst %.cc,build/%.o,$(CONV_OSM_SRC)) $(PROTO_OBJ)
WAYINT_OBJ    = $(patsubst %.cc,build/%.o,$(WAYINT_SRC))
TILER_OBJ    = $(patsubst %.cc,build/%.o,$(TILER_SRC))
#CONV_OBJ = $(patsubst %.cc,build/%.o,$(CONV_SRC))
#SIMP_OBJ = $(patsubst %.cc,build/%.o,$(SIMP_SRC))

FLAGS = -g -Wall -Wextra #-DNDEBUG -fsanitize=address -fno-omit-frame-pointer #-O2 -flto
#FLAGS = -ftrapv -g -Wall -Wextra 
#FLAGS = -ftrapv -g -Wall -Wextra -fprofile-arcs -ftest-coverage
CFLAGS = $(FLAGS) -std=c99
CCFLAGS = $(FLAGS) -std=c++11
LD_FLAGS = #-fsanitize=address#-flto -O2 #-fprofile-arcs#--as-needed
.PHONY: all clean
.SECONDARY: $(PROTO_SRC)
all: build make.dep build/conv_osm build/wayInt build/tiler intermediate  
#build/simplifier build/data_converter
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

build/tiler: $(TILER_OBJ)
	@echo [LD ] $@
	@g++ $^  $(LD_FLAGS) -o $@

build/wayInt: $(WAYINT_OBJ)
	@echo [LD ] $@
	@g++ $^  $(LD_FLAGS) -o $@

build/conv_osm: $(CONV_OSM_OBJ)
	@echo [LD ] $@
	@g++ $^ $(LD_FLAGS) -lprotobuf -lz -o $@


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

make.dep: $(CONV_OSM_SRC) $(WAYINT_SRC) $(TILER_SRC)
	@echo [DEP]
	@g++ -MM -MG $^ | sed "s/\([[:graph:]]*\)\.o/build\/\\1.o/g" > make.dep

include make.dep

