
OSM_C_SRC = mem_map.c
OSM_CC_SRC = filter_osm.cc

OSM_SRC = $(OSM_C_SRC) $(OSM_CC_SRC)

#COAST_SRC = filter_coastline.cc
#OSM_OBJS = filter_osm.o

CFLAGS = -g -Wall -Wextra -O2
CCFLAGS = $(CFLAGS)

.PHONY: all clean


all: filter_osm dump_kv
#	 @echo [ALL] $<

filter_osm: mem_map.o filter_osm.o osm_tags.o
	@echo [LD] $@
	@g++ $(CCFLAGS) $^ -o $@

filter_osm.o: filter_osm.cc
	@echo [C+] $<
	@g++ $(CCFLAGS) $< -c -o $@

mem_map.o: mem_map.c
	@echo [CC] $<
	@gcc $(CFLAGS) $< -c -o $@

osm_tags.o: osm_tags.c osm_tags.h
	@echo [CC] $<
	@gcc $(CFLAGS) $< -c -o $@

dump_kv: dump_kv.c
	@echo [CC] $@
	@gcc $(CFLAGS) $< -o $@

clean:
	@echo [CLEAN]
	@rm *~ *.o
	@rm filter_osm
#filter_coastline: $(COAST_SRC)
#	g++ -g $(CCFLAGS) $(COAST_SRC) -o $@


