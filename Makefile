
OSM_SRC = filter_osm.cc
#COAST_SRC = filter_coastline.cc
#OSM_OBJS = filter_osm.o

CCFLAGS = -g -Wall -Wextra #-O2

.PHONY: all


all: filter_osm
	 @echo [ALL]

filter_osm: $(OSM_SRC)
	g++ $(CCFLAGS) $(OSM_SRC) -o $@


#filter_coastline: $(COAST_SRC)
#	g++ -g $(CCFLAGS) $(COAST_SRC) -o $@


