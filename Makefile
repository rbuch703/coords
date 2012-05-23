
OSM_SRC = filter_osm.cc mem_map.cc osm_types.cc osm_tags.cc
CONV_SRC = data_converter.cc osm_types.cc mem_map.cc geometric_types.cc # geometry_processing.cc

OSM_OBJ = $(OSM_SRC:.cc=.o)
CONV_OBJ = $(CONV_SRC:.cc=.o)

#COAST_SRC = filter_coastline.cc
#OSM_OBJS = filter_osm.o

CFLAGS = -g -Wall -Wextra #-O2
CCFLAGS = $(CFLAGS)

.PHONY: all clean


all: filter_osm dump_kv data_converter
#	 @echo [ALL] $<

filter_osm: $(OSM_OBJ) make.dep
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(OSM_OBJ) -o $@

data_converter: $(CONV_OBJ) make.dep
	@echo [LD ] $@
	@g++ $(CCFLAGS) $(CONV_OBJ) -o $@

%.o: %.cc
	@echo [C++] $<
	@g++ $(CCFLAGS) $< -c -o $@

dump_kv: dump_kv.c
	@echo [LD ] $@
	@gcc $(CFLAGS) $< -o $@

clean:
	@echo [CLEAN]
	@test -f filter_osm && rm filter_osm
	@test -f dump_kv && rm dump_kv
	@test -f make.dep && rm make.dep
	@rm *.o
	@rm *~

make.dep: $(OSM_SRC) $(CONV_SRC)
	@echo [DEP]
	@g++ -MM $(OSM_SRC) $(CONV_SRC) > make.dep

include make.dep

