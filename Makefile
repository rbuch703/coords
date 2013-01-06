
CONV_XML_SRC = conv_osmxml.cc mem_map.cc osm_types.cc osm_tags.cc osmxmlparser.cc #geometric_types.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc helpers.cc
SIMP_SRC = simplifier.cc osm_types.cc geometric_types.cc polygonreconstructor.cc mem_map.cc helpers.cc validatingbigint.cc int128.cc
GEO_SRC = geo_unit_tests.cc geometric_types.cc simplifypolygon.cc validatingbigint.cc int128.cc
GL_TEST_SRC = gl_test.cc geometric_types.cc validatingbigint.cc int128.cc


CONV_XML_OBJ  = $(CONV_XML_SRC:.cc=.o)
CONV_OBJ = $(CONV_SRC:.cc=.o)
SIMP_OBJ = $(SIMP_SRC:.cc=.o)
GEO_OBJ  = $(GEO_SRC:.cc=.o)
GL_TEST_OBJ = $(GL_TEST_SRC:.cc=.o)

CFLAGS = -g -Wall -Wextra -std=c++11 #-O2
CCFLAGS = $(CFLAGS)
LD_FLAGS = #--as-needed
.PHONY: all clean

all: make.dep conv_osmxml data_converter simplifier geo_unit_tests gl_test
#	 @echo [ALL] $<

gl_test: $(GL_TEST_OBJ)
	@echo [LD ] $@
	@g++ $(GL_TEST_OBJ) $(CCFLAGS) $(LD_FLAGS) -lGL -lglfw -lgmp -lgmpxx -o $@

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
	@rm *.o
	@rm *~
	@rm conv_osmxml data_converter simplifier geo_unit_tests gl_test

make.dep: $(CONV_XML_SRC) $(CONV_SRC) $(SIMP_SRC) $(GEO_SRC) $(GL_TEST_SRC)
	@echo [DEP]
	@g++ -MM $^ > make.dep

include make.dep

