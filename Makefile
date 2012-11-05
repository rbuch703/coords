
CONV_XML_SRC = conv_osmxml.cc mem_map.cc osm_types.cc osm_tags.cc geometric_types.cc osmxmlparser.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc geometric_types.cc helpers.cc
STATS_SRC = statistics_xmlosm.cc osm_types.cc osmxmlparser.cc mem_map.cc geometric_types.cc
SIMP_SRC = simplifier.cc osm_types.cc geometric_types.cc polygonreconstructor.cc mem_map.cc helpers.cc
GEO_SRC = geo_unit_tests.cc geometric_types.cc 

CONV_XML_OBJ  = $(CONV_XML_SRC:.cc=.o)
CONV_OBJ = $(CONV_SRC:.cc=.o)
SIMP_OBJ = $(SIMP_SRC:.cc=.o)
GEO_OBJ  = $(GEO_SRC:.cc=.o)
STATS_OBJ  = $(STATS_SRC:.cc=.o)

#COAST_SRC = filter_coastline.cc
#OSM_OBJS = filter_osm.o

CFLAGS = -g -Wall -Wextra #-O2
CCFLAGS = $(CFLAGS)

.PHONY: all clean


all: make.dep conv_osmxml data_converter simplifier geo_unit_tests stats_xmlosm
#	 @echo [ALL] $<

conv_osmxml: $(CONV_XML_OBJ) 
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(CONV_XML_OBJ) -o $@

stats_xmlosm: $(STATS_OBJ) 
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(STATS_OBJ) -o $@

data_converter: $(CONV_OBJ) 
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(CONV_OBJ) -o $@

simplifier: $(SIMP_OBJ)
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(SIMP_OBJ) -o $@

geo_unit_tests: $(GEO_OBJ)
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(GEO_OBJ) -o $@

#dump_kv: dump_kv.c
#	@echo [LD ] $@
#	@gcc $(CFLAGS) $< -o $@

%.o: %.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) $< -c -o $@

clean:
	@echo [CLEAN]
	@rm *.o
	@rm *~
	@rm conv_osmxml data_converter simplifier geo_unit_tests

make.dep: $(CONV_XML_SRC) $(CONV_SRC) $(SIMP_SRC) $(GEO_SRC)
	@echo [DEP]
	@g++ -MM $^ > make.dep

include make.dep

