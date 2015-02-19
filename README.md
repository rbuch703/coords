This software is licensed under the GNU Affero General Public License version 3 (GNU AGPL-v3).

This repository consists of tools that together create a OSM data storage from an OSM PBF file (e.g. a planet dump, or a regional extract). This data storage can then be used to render OSM data using Mapnik alone, or to create an OSM tile server using Mapnik with renderd and mod_tile. 

Please note: These tools are prototypes and not yet ready for production use. Use at your own risk!

Building Instructions (for Ubuntu 14.04)
----------------------------------------
* `sudo apt-get install make build-essential libprotobuf-dev protobuf-compiler libexpat1-dev cmake` # install the required dependencies
* `git clone https://github.com/rbuch703/coords.git` # clone the repository
* `cd coords`
* `cmake .`
* `make` # compile the tools

The tools will be build in the `build` directory

Execution Instructions
----------------------

Creating a COORDS data storage requires three steps that need to be executed in order. In short, these are:

1. `./coordsCreateDatabase <pbf file>` (for full planet dumps and continent-sized extracts), or `coordsCreateDatabase --remap <pbf file>` (for regional extracts that are country-sized or smaller)
2. `./coordsResolveLocations` 
3. `./coordsCreateTiles`

However, run without additional parameters, this may take much longer than necessary, at least for planet dump `pbf`s. The next three sections explain what these tools do in some detail, and give performance tuning tips.

### 1. Create Entity Indices (coordsCreateDatabase)


This step uses the `coordsCreateDatabase` tool to transform all nodes, ways and relations into a form where they can efficiently be accessed by their respective ID. It also creates a lookup table to easily find the lat/lng geographic coordinates of a given node ID. This step is performed by executing

`./coordsCreateDatabase <pbf file>`

, where `<pbf file>` is the path to the OSM PBF file whose contents you wish to convert to a COORDS data storage.

There is one optional parameter `--remap` to `coordsCreateDatabase` that will change all way, node and relation IDs to be more compact. With this option, `coordsCreateDatabase` uses more RAM (up to an additional 2GB for each 100 million nodes/ways/relations). But for regional data extracts, this massively reduces hard disk space consumption, and also the RAM requirement for step 2. It is therefore suggested to use `./coordsCreateDatabase --remap <pbf file>` when handling extracts of countries and smaller regions, and to omit the `--remap` flag for full planet dumps.
<!--- This step creates huge index tables (the `*.idx` files in the `intermediate` directory), which require 8 bytes of storage space for each **possible** entity ID up to the maximum ID present. For example, in a recent 2014 planet dump there are about 2.7 billion nodes, with a maximum node ID of about 3.4 billion. So the node index table will have `27.2 GB` (3.4 billion IDs times 8 bytes). Small regional extracts (like the UK extract from http://download.geofabrik.de), however, will contain much fewer entities (e.g. 68 million nodes for the UK), but still have the same maximum entity IDs. So the node index table for the UK will also have `27.2GB`, even though only about `500MB` of it are actually used! Even worse, the next processing step uses about as much RAM as this index table is big, so processing a UK regional extract would require about 27GB of free RAM.   -->

### 2. Added Geographic Locations to Stored Ways (coordsResolveLocations)

OpenStreetMap ways have no geographic locations by themselves. Instead, they consist of a list of node references, and the nodes in turn hold the geographic information (a latitude/longitude pair). Having to look up all referenced nodes whenever one deals with a given way would be extremely slow and inefficient. The second tool, `coordsResolveLocations`, instead looks up all nodes referenced by all ways at once, and adds the nodes' geographic locations directly to the stored ways. The basic way to do this is to execute

`./coordsResolveLocations`

. This works fine for small regional extracts (up to country size), but will likely be very slow for full planet dumps: This approach loads each node into memory once it is needed, which results in many very slow random seeks on the hard drive (SSDs are better at random seeks, but still rather slow). The tool `coordsResolveLocations` has an alternative `BLOCK` mode that works better in situations where many nodes have to be read, but don't all fit into memory at the same time. In `BLOCK` mode, the set of all nodes is partitioned into sets of a fixed size (given by the parameter `--lock`). Each block is read into memory in turn, and for each block, *all* ways are processed, but only those node reference referring ot nodes in the current block are updated with geographic information. So if the set of nodes is partitioned into five blocks, the whole set of ways has to be read and updated five times.

The `BLOCK` mode with a big-enough block size can be orders of magnitude faster that the basic random access mode. But `BLOCK` mode requires the current block to be `mlock()`'ed in memory, something only root users or users with advances permissions (`CAP_SYS_RESOURCE`) can do. So to run `coordsResolveLocations` with a block size of 16GB (=16000MB) as root, execute

`sudo ./coordsResolveLocations --mode BLOCK --lock 16000`

The block size should be as big as possible, but needs to be smaller that the amount of physically present RAM (not counting swap space). The number given is the maximum permissible block size. The tool `coordsResolveLocations` will reduce the actually used block size if it can do so without affecting performance (e.g. in order to process 28GB of node data with a maximum user-selected block size of `5.0GB` requires a subdivision into `ceil(28/5) = 6` node blocks. But evenly distributing the node across six node blocks requires each block to only be`28/6 = 4.67GB` big, so `coordsResolveLocations` will only use `4.67GB` instead of the allow `5.0GB`). A good rule of thumb is to use a block size of about `1-2GB` less than the available physical RAM. To check how many iterations are necessary for the current data set with a selected block size, add the `--dryrun` parameter to `coordsResolveLocations`. This will cause `coordsResolveLocations` to print run statistics without performing the actual computations.

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

1. `coordsCreateDatabase`: about 1.5h
2. `coordsResolveLocations`: about 2h for two passes (i.e. using the parameter `--mode=BLOCK --lock=16000`)
3. `coordsCreateTiles`: about 2h
 
