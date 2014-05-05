
"osm": various tools for OpenStreetMap data conversion and geometric processing

Dependencies:
- g++ (for use with the supplied Makefile)
- GNU make
- nasm
- headers and libraries for gmp, opengl, glfw, cairo and curl 

Build instructions:
- "make"

Compilation Output:
- "conv_osmxml": converts OpenStreetMap XML files (e.g. planet dumps and regional extracts) to a form that allows for random access. This is not a standard-conformant XML DOM or SAX-parser, but a specialized parser to efficiently process the huge amount of OpenStreetMap data. This process may take an hour at 32GB of RAM, and up to a day for systems with fewer RAM writing on a HDD instead of a SSD.
- "data_converter": extracts several categories of data (e.g. streets, buildings) from the random access data files
- "simplifier": processes select geometry (e.g. coastlines, buildings) from the random access and categorical data files to individual tiles for all relevant zoom levels. This may take about half a day.
- "tests/*": several algorithm and unit tests
- "make_idx": creates index files for the tile hierarchies created by "simplifier" as directories for how deep the tile hierarchy is in which regions of the world.

Usage:
- conv_osmxml: this tool assumes that the OSM xml is passed via stdin. This allows for easy on-the-flight decompression of the OSM planet dump. Given a planet dump or another OSM extract (e.g. from http://wiki.openstreetmap.org/wiki/Planet.osm or http://download.geofabrik.de/ such as planet-140423.osm.bz2), this tool would convert all contained geographic data to the random access representation using "bzcat planet-140423.osm.bz2 | ./conv_osmxml". 
- "./data_converter" uses hardcoded input files (those created by conv_osmxml) and hardcoded output files
- "./simplifier" uses hardcoded input files (those created by conv_osmxml and data_converter) and hardcoded output paths
- "./make_idx" uses hardcoded input files (those created by "simplifier") and hard-coded output files.

Output:
- vector tiles (about 150000 in case of processing the planet dump) and the associated index files for visualization using the vector tiles web application
