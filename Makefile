
CONV_XML_SRC = conv_osmxml.cc mem_map.cc osm_types.cc osm_tags.cc osmParserXml.cc osmformat.pb.cc #geometric_types.cc
#REMAP_SRC = remapper.cc mem_map.cc osm_types.cc osm_tags.cc osmxmlparser.cc idRemappingParser.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc
SIMP_SRC = simplifier.cc osm_types.cc mem_map.cc

CONV_XML_OBJ  = $(patsubst %.cc,build/%.o,$(CONV_XML_SRC))
#REMAP_OBJ  = $(REMAP_SRC:.cc=.o)
CONV_OBJ = $(patsubst %.cc,build/%.o,$(CONV_SRC))
SIMP_OBJ = $(patsubst %.cc,build/%.o,$(SIMP_SRC))

FLAGS = -g -Wall -Wextra -DNDEBUG -O2 -flto
#FLAGS = -ftrapv -g -Wall -Wextra 
#FLAGS = -ftrapv -g -Wall -Wextra -fprofile-arcs -ftest-coverage
CFLAGS = $(FLAGS) -std=c99
CCFLAGS = $(FLAGS) -std=c++11
LD_FLAGS = -flto -O2 #-fprofile-arcs#--as-needed
.PHONY: all clean

all: build make.dep build/conv_osmxml build/simplifier build/data_converter intermediate 
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

build/conv_osmxml: $(CONV_XML_OBJ)
	@echo [LD ] $@
	@g++ $^ $(LD_FLAGS) -lprotobuf -o $@

build/data_converter: $(CONV_OBJ)
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(LD_FLAGS) $^ ../GeoLib/build/geolib.a -lgmp -lgmpxx -o $@

build/simplifier: $(SIMP_OBJ)
	@echo [LD ] $@
	@g++ $^ $(CCFLAGS) $(LD_FLAGS) ../GeoLib/build/geolib.a -lgmp -lgmpxx -o $@

build/data_converter.o: data_converter.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) -I../GeoLib $< -c -o $@

build/simplifier.o: simplifier.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) -I../GeoLib $< -c -o $@


build/%.o: %.cc 
	@echo [C++] $<
#	@g++ $(CCFLAGS) `pkg-config --cflags cairo` $< -c -o $@
	@g++ $(CCFLAGS) $< -c -o $@

%.pb.cc: %.proto
	@echo [PBC] $<
	@protoc --cpp_out=. $<

clean:
	@echo [CLEAN]
	@rm -rf *~
	@rm -rf *gcda
	@rm -rf *gcno
	@rm -rf conv_osmxml data_converter simplifier make_index remap
	@rm -rf coverage.info callgrind.out.*
	@rm -rf build/*

make.dep: $(CONV_XML_SRC) $(CONV_SRC) $(SIMP_SRC) $(GEO_SRC) 
	@echo [DEP]
	@g++ -MM $^ | sed "s/\([[:graph:]]*\)\.o/build\/\\1.o/g" > make.dep

include make.dep

