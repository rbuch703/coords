
OSM_C_SRC = mem_map.c
OSM_CC_SRC = filter_osm.cc

OSM_SRC = $(OSM_C_SRC) $(OSM_CC_SRC)

#COAST_SRC = filter_coastline.cc
#OSM_OBJS = filter_osm.o

CFLAGS = -g -Wall -Wextra #-O2
CCFLAGS = $(CFLAGS)

.PHONY: all clean


all: filter_osm
#	 @echo [ALL] $<

filter_osm: mem_map.o filter_osm.o
	@echo [LD] $@
	@g++ $(CCFLAGS) mem_map.o filter_osm.o -o $@

filter_osm.o: filter_osm.cc
	@echo [C+] $<
	@g++ $(CCFLAGS) $< -c -o $@

mem_map.o: mem_map.c
	@echo [CC] $<
	@gcc $(CFLAGS) $< -c -o $@
    
clean:
	@echo [CLEAN]
	@rm *~ *.o
	@rm filter_osm
#filter_coastline: $(COAST_SRC)
#	g++ -g $(CCFLAGS) $(COAST_SRC) -o $@


