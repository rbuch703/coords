This software is licensed under the GNU Affero General Public License version 3 (GNU AGPL-v3).

This repository consists of tools that together create a COORDS ("chunk-oriented OSM render-data storage") data storage from an OSM PBF file (e.g. a planet dump, or a regional extract). This data storage can then be used to render OSM data using Mapnik alone, or to create an OSM tile server using Mapnik with renderd and mod_tile. 

Please note: These tools are prototypes and not yet ready for production use. Use at your own risk!



Building Instructions (for Ubuntu 14.04)
----------------------------------------
* `sudo apt-get install make build-essential libprotobuf-dev protobuf-compiler libexpat1-dev cmake` # install the required dependencies
* `git clone https://github.com/rbuch703/coords.git` # clone the repository
* `cd coords`
* `cmake .`
* `make` # compile the tools

Execution Instructions
----------------------

Creating a COORDS data storage requires three steps that need to be executed in order. In short, these are:

1. `./coordsCreateStorage <pbf file>` (for full planet dumps and continent-sized extracts), or `coordsCreateStorage --remap <pbf file>` (for regional extracts that are country-sized or smaller)
2. `./coordsResolveLocations` 
3. `./coordsCreateTiles`

However, run without additional parameters, this may take much longer than necessary, at least for planet dump `pbf`s. The next three sections explain what these tools do in some detail, and give performance tuning tips.

### 1. Create Entity Indices (coordsCreateStorage)


This step uses the `coordsCreateStorage` tool to transform all nodes, ways and relations into a form where they can efficiently be accessed by their respective ID. It also creates a lookup table to easily find the lat/lng geographic coordinates of a given node ID. This step is performed by executing

`./coordsCreateStorage <pbf file>`

, where `<pbf file>` is the path to the OSM PBF file whose contents you wish to convert to a COORDS data storage.


### 2. Added Geographic Locations to Stored Ways (coordsResolveLocations)

OpenStreetMap ways have no geographic locations by themselves. Instead, they consist of a list of node references, and the nodes in turn hold the geographic information (a latitude/longitude pair). Having to look up all referenced nodes whenever one deals with a given way would be extremely slow and inefficient. The second tool, `coordsResolveLocations`, instead looks up all nodes referenced by all ways at once, and adds the nodes' geographic locations directly to the stored ways. The basic way to do this is to execute

`./coordsResolveLocations`


### 3. Creating the Vector Tiles (coordsCreateTiles)
3. `coordsCreateTiles`

The third step subdivides the data in the updated way index into a hierarchy of tiles of equal data size (not necessarily equal geographic coverage). This hierarchy can then be used by Mapnik plugins to efficiently retrieve the data required to render a given region (the required plugin can be found in the github repository `rbuch703/coords-mapnik-plugin`). It has no parameters, and can simply be executed as
* `./coordsCreateTiles`


RAM Requirements
================
The tools themselves require little memory (< 100MB), but will benefit greatly from big caches. The performance of the tools will increase dramatically when the following requirements are met:

1. enough free RAM hold the complete program in memory, about 100 MB
2. enough free RAM for read-ahead and write caches, about 1GB
3. enough free RAM for read-ahead and write caches, as well as for complete memory mapping to the node ID->lat/lng index. The exact amount depends on the particular PBF file used as a data source. For a 2015 Planet dump, this is about 28GB of RAM. 

Disk Space Requirements
=======================
The tools write various data storage files (the memory requirements here are given for a typical 2015 Planet dump. They will be significatly lower for regional extracts):

1. the complete node, way and relation indices (`nodes.idx, nodes.data, ways.idx, ways.data, relations.idx and relations.data` in the `intermediate` directory): about 150 GB
2. the node ID-> lat/lng index (`vertices.data` in the `intermediate` directory): about 28GB
3. The vector data tiles (numbered files in the `nodes` directory): ~70GB

So the whole COORDS storage requires about 300GB of disk space for a planet dump.

Execution Time
==============
The execution times for these tools varies dramatically depending on:

* how much data the input PBF file contains
* whether you are using the ID remapper
* how much RAM is available (see "RAM Requirements")
* whether the files are stored on a hard disk, or an SSD

The following execution times where measured on a 2x2.4GHz virtual machine with 32GB of RAM and a hard drive-backed file system, processing a recent Planet dump (28.5GB) without ID remapping:

1. `coordsCreateStorage`: about 1.5h
2. `coordsResolveLocations --mode=BLOCK --lock=16000`: about 2h for two passes 
3. `coordsCreateTiles`: about 2h
 
