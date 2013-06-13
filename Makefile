
CONV_XML_SRC = conv_osmxml.cc mem_map.cc osm_types.cc osm_tags.cc osmxmlparser.cc #geometric_types.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc helpers.cc
SIMP_SRC = simplifier.cc osm_types.cc geometric_types.cc vertexchain.cc polygonreconstructor.cc mem_map.cc helpers.cc quadtree.cc int128.cc triangulation.cc
GL_TEST_SRC = gl_test.cc geometric_types.cc int128.cc vertexchain.cc triangulation.cc helpers.cc
TEST_SRC = tests/arithmetic_test.cc tests/geometry_test.cc tests/quadtree_test.cc tests/triangulation_test.cc

CONV_XML_OBJ  = $(CONV_XML_SRC:.cc=.o)
CONV_OBJ = $(CONV_SRC:.cc=.o)
SIMP_OBJ = $(SIMP_SRC:.cc=.o) math64.o
GEO_OBJ  = $(GEO_SRC:.cc=.o) math64.o
GL_TEST_OBJ = $(GL_TEST_SRC:.cc=.o) math64.o

# -ftrapv is extremely important to catch integer overflow bugs, which are otherwise hard to detect
# OSM data covers almost the entire range of int32_t; multiplying two values (required for some algorithms)
# already uses all bits of an int64_t, so, more complex algorithms could easily cause an - otherwise undetected -
# integer overflow
# WARNING: the gcc option -O2 appears to negate the effects of -ftrapv ! 
#FLAGS = -g -Wall -Wextra -DNDEBUG -O2
FLAGS = -ftrapv -g -Wall -Wextra 
#FLAGS = -ftrapv -g -Wall -Wextra -fprofile-arcs -ftest-coverage
CFLAGS = $(FLAGS) -std=c99
CCFLAGS = $(FLAGS) -std=c++11
LD_FLAGS = #-fprofile-arcs#--as-needed
.PHONY: all clean

all: make.dep conv_osmxml data_converter simplifier gl_test make_index tests
#	 @echo [ALL] $<

make_index: make_index.cc
	@echo [LD ] $@
	@g++ $^ -o $@

gl_test: $(GL_TEST_OBJ)
	@echo [LD ] $@
	@g++ $^  $(LD_FLAGS) `curl-config --libs` -lGL -lglfw -lm -o $@

conv_osmxml: $(CONV_XML_OBJ)
	@echo [LD ] $@
	@g++ $^ $(LD_FLAGS) -o $@

data_converter: $(CONV_OBJ)
	@echo [LD ] $@
	@g++ $^ $(CCFLAGS) $(LD_FLAGS) -lgmp -lgmpxx -o $@

simplifier: $(SIMP_OBJ)
	@echo [LD ] $@
	@g++ $^ $(CCFLAGS) $(LD_FLAGS) -lgmp -lgmpxx -o $@

tests: tests/arithmetic_test tests/geometry_test tests/quadtree_test tests/triangulation_test

tests/arithmetic_test: math64.o validatingbigint.o int128.o tests/arithmetic_test.cc
	@echo [LD ] $@
	@g++ math64.o validatingbigint.o int128.o tests/arithmetic_test.cc $(CCFLAGS) $(LD_FLAGS) -lgmp -lgmpxx -o $@ 

tests/geometry_test: math64.o int128.o quadtree.o vertexchain.o geometric_types.o tests/geometry_test.cc
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(LD_FLAGS) -o $@ math64.o int128.o quadtree.o vertexchain.o geometric_types.o tests/geometry_test.cc

tests/quadtree_test: tests/quadtree_test.cc geometric_types.o vertexchain.o int128.o quadtree.o math64.o
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(LD_FLAGS) -o $@ $^ 

tests/triangulation_test: tests/triangulation_test.cc geometric_types.o vertexchain.o int128.o math64.o quadtree.o triangulation.o helpers.o
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(LD_FLAGS) -o $@ $^ `pkg-config --cflags --libs cairo`


math64.o: math64.asm
	@echo [ASM] $<
	@nasm -f elf64 -o $@ $<

%.o: %.cc
	@echo [C++] $<
#	@g++ $(CCFLAGS) `pkg-config --cflags cairo` $< -c -o $@
	@g++ $(CCFLAGS) $< -c -o $@

	
clean:
	@echo [CLEAN]
	@rm -rf *.o
	@rm -rf *~
	@rm -rf tests/*~
	@rm -rf *gcda
	@rm -rf *gcno
	@rm -rf conv_osmxml data_converter simplifier geo_unit_tests make_index
	@rm -rf gl_test tests/arithmetic_test tests/geometry_test tests/quadtree_test tests/triangulation_test
	@rm -rf coverage.info callgrind.out.*

make.dep: $(CONV_XML_SRC) $(CONV_SRC) $(SIMP_SRC) $(GEO_SRC) $(TEST_SRC) $(GL_TEST_SRC)
	@echo [DEP]
	@g++ -MM $^ > make.dep

include make.dep

