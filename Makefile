
OSM_SRC = filter_osm.cc mem_map.cc osm_types.cc osm_tags.cc geometric_types.cc osmxmlparser.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc geometric_types.cc polygonreconstructor.cc
SIMP_SRC = simplifier.cc osm_types.cc geometric_types.cc polygonreconstructor.cc mem_map.cc
GEO_SRC = geo_unit_tests.cc geometric_types.cc

OSM_OBJ  = $(OSM_SRC:.cc=.o)
CONV_OBJ = $(CONV_SRC:.cc=.o)
SIMP_OBJ = $(SIMP_SRC:.cc=.o)
GEO_OBJ  = $(GEO_SRC:.cc=.o)

#COAST_SRC = filter_coastline.cc
#OSM_OBJS = filter_osm.o

CFLAGS = -g -Wall -Wextra #-O2
CCFLAGS = $(CFLAGS)

.PHONY: all clean


all: make.dep filter_osm dump_kv data_converter simplifier geo_unit_tests
#	 @echo [ALL] $<

filter_osm: $(OSM_OBJ) 
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(OSM_OBJ) -o $@

data_converter: $(CONV_OBJ) 
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(CONV_OBJ) -o $@

simplifier: $(SIMP_OBJ)
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(SIMP_OBJ) -o $@

geo_unit_tests: $(GEO_OBJ)
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(GEO_OBJ) -o $@

dump_kv: dump_kv.c
	@echo [LD ] $@
	@gcc $(CFLAGS) $< -o $@

%.o: %.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) $< -c -o $@

clean:
	@echo [CLEAN]
	@test -f filter_osm && rm filter_osm
	@test -f data_converter && rm data_converter
	@test -f simplifier && rm simplifier
	@test -f dump_kv && rm dump_kv
	@test -f make.dep && rm make.dep
	@rm *.o
	@rm *~

make.dep: $(OSM_SRC) $(CONV_SRC) $(SIMP_SRC) $(GEO_SRC)
	@echo [DEP]
	@g++ -MM $^ > make.dep

include make.dep

