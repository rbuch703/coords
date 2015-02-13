This software is licensed under the GNU Affero General Public License version 3 (GNU AGPL-v3).

This repository consists of various tools that together create a OSM data storage from an OSM PBF file (e.g. a planet dump, or a regional extract). This data storage can then be used to render OSM data using Mapnik alone, or to create an OSM tile server using Mapnik with renderd and mod_tile. 

Please note: These tools are prototypes and not yet ready for production use. The tools are incomplete, and many settings can only be made by modifying source code. The intended audience at this point are therefore software developers with at least a basic understanding of C++. 

Building Instructions (for Ubuntu 14.04)
----------------------------------------
* install the required dependencies, e.g. `sudo apt-get install make build-essential libprotobuf-dev protobuf-compiler libexpat1-dev`
* compile the tools using `make` (there is currently no `./configure` step)
The tools will be build in the `build` directory

Execution Instructions
----------------------

Creating a COORDS data storage requires three steps that need to be executed in order:

### 1. Create Entity Indices


This step uses the `conv_osm` tool to transform all nodes, ways and relations into a form where they can efficiently be accessed by there respective ID. It also creates a lookup table to easily find the lat/lng geographic coordinates of a given node ID. This step is usually performed by executing

`build/conv_osm <pbf file>`

There is one optional parameter `--remap` to `conv_osm` that will change all way and node IDs to be more compact. With this option, `conv_osm` uses more RAM (up to an additional `1GB` for each `100M` nodes and ways), and the resulting index cannot easily be updated later. But for regional data extracts, this massively reduces hard disk space consumption, and also the RAM requirement for step 2. It is therefore suggested to use `build/conv_osm --remap <pbf file>` when handling extracts of countries and smaller regions, and to omit the `--remap` flag for full planet dumps.
<!--- This step creates huge index tables (the `*.idx` files in the `intermediate` directory), which require 8 bytes of storage space for each **possible** entity ID up to the maximum ID present. For example, in a recent 2014 planet dump there are about 2.7 billion nodes, with a maximum node ID of about 3.4 billion. So the node index table will have `27.2 GB` (3.4 billion IDs times 8 bytes). Small regional extracts (like the UK extract from http://download.geofabrik.de), however, will contain much fewer entities (e.g. 68 million nodes for the UK), but still have the same maximum entity IDs. So the node index table for the UK will also have `27.2GB`, even though only about `500MB` of it are actually used! Even worse, the next processing step uses about as much RAM as this index table is big, so processing a UK regional extract would require about 27GB of free RAM.   -->

2. `build/wayInt`
3. `build/tiler`

The first step creates full covering indices for all OSM nodes, ways and relations contained in the PBF file, mapping their IDs to their full data. It also creates an index mapping all node IDs to their corresponding geographic coordinates (lat/lng). The second step uses the node ID-> lat/lng index to update the way storage, adding the node positions (lat/lng) to each node reference contained in a way. The third step subdivides the data in the updated way index into a hierarchy of tiles of equal data size (not necessarily equal geographic coverage). This hierarchy can then be used by Mapnik plugins to efficiently retrieve the data required to render a given region (the required plugin can be found in the github repository `rbuch703/coords-mapnik-plugin`).


RAM Requirements
================
The tools themselves require little memory (< 100MB), but will benefit greatly from big caches. The performance of the tools will increase dramatically when the following requirements are met:

1. enough free RAM hold the complete program in memory, about 100 MB
2. enough free RAM for read-ahead and write caches, about 1GB
3. enough free RAM for read-ahead and write caches, as well as for complete memory mapping to the node ID->lat/lng index. The exact amount depends on the particular PBF file used as a data source. For a 2015 Planet dump, this is about 28GB of RAM. 

Disk Storage Requirements
=========================
The tools write various data storage files (the memory requirements here are given for a typical 2015 Dlanet dump. They will be significatly lower for regional extracts):

1. the complete node, way and relation indices (`nodes.idx, nodes.data, ways.idx, ways.data, relations.idx and relations.data` in the `intermediate` directory): about 150 GB
2. the node ID-> lat/lng index (`vertices.data` in the current working directory): about 20GB
3. The vector data tiles (numbered files in the `nodes` directory): ~70GB


Execution Time
==============
The execution times for these tools varies dramatically depending on

* how much data the input PBF file contains
* whether you are using the ID remapper
* how much RAM is available (see "RAM Requirements")
* whether the files are stored on a hard disk, or an SSD

The following execution times where measured on a 2x2.4GHz virtual machine with 32GB of RAM and a hard drive-backed file system, processing a recent Planet dump (28.5GB) without ID remapping:

1. `build/conv_osm`: about 1.5h
2. `build/wayInt`: about 2h for two passes
3. `build/tiles`: about XXX
 
