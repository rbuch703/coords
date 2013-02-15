
CONV_XML_SRC = conv_osmxml.cc mem_map.cc osm_types.cc osm_tags.cc osmxmlparser.cc #geometric_types.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc helpers.cc
SIMP_SRC = simplifier.cc osm_types.cc geometric_types.cc vertexchain.cc polygonreconstructor.cc mem_map.cc helpers.cc validatingbigint.cc int128.cc
GEO_SRC = geo_unit_tests.cc geometric_types.cc vertexchain.cc int128.cc #validatingbigint.cc 
GL_TEST_SRC = gl_test.c #geometric_types.cc validatingbigint.cc int128.cc


CONV_XML_OBJ  = $(CONV_XML_SRC:.cc=.o)
CONV_OBJ = $(CONV_SRC:.cc=.o)
SIMP_OBJ = $(SIMP_SRC:.cc=.o)
GEO_OBJ  = $(GEO_SRC:.cc=.o)

FLAGS = -g -Wall -Wextra  -fprofile-arcs -ftest-coverage #-O2
CFLAGS = $(FLAGS) -std=c99
CCFLAGS = $(FLAGS) -std=c++11
LD_FLAGS = -fprofile-arcs#--as-needed
.PHONY: all clean

all: make.dep conv_osmxml data_converter simplifier geo_unit_tests gl_test
#	 @echo [ALL] $<

gl_test: $(GL_TEST_SRC)
	@echo "[C  ]" $@
	@gcc $(CFLAGS) $(LD_FLAGS) -lGL -lglfw -lm $(GL_TEST_SRC) -o $@

conv_osmxml: $(CONV_XML_OBJ) 
	@echo [LD ] $@
	@g++ $(CONV_XML_OBJ) $(LD_FLAGS) -o $@

data_converter: $(CONV_OBJ) 
	@echo [LD ] $@
	@g++ $(CONV_OBJ) $(CCFLAGS) $(LD_FLAGS) -o $@

simplifier: $(SIMP_OBJ)
	@echo [LD ] $@
	@g++ $(SIMP_OBJ) $(CCFLAGS) $(LD_FLAGS) -lgmp -lgmpxx -o $@

geo_unit_tests: $(GEO_OBJ)
	@echo [LD ] $@
	@g++ $(GEO_OBJ) $(CCFLAGS) $(LD_FLAGS) -lgmp -lgmpxx -o $@

%.o: %.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) $< -c -o $@

clean:
	@echo [CLEAN]
	@rm -rf *.o
	@rm -rf *~
	@rm -rf conv_osmxml data_converter simplifier geo_unit_tests gl_test

make.dep: $(CONV_XML_SRC) $(CONV_SRC) $(SIMP_SRC) $(GEO_SRC)
	@echo [DEP]
	@g++ -MM $^ > make.dep

include make.dep

