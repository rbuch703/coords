This software is licensed under the GNU Affero General Public License version 3 (GNU AGPL-v3).

This repository consists of tools that together create a COORDS ("chunk-oriented OSM render-data storage") data storage from an OSM PBF file (e.g. a planet dump, or a regional extract). This data storage can then be used to render OSM data using Mapnik alone, or to create an OSM tile server using Mapnik with renderd and mod_tile. 

Please note: These tools are prototypes and not yet ready for production use. Use at your own risk!

Build Instructions (for Ubuntu 14.04)
----------------------------------------
* `sudo apt-get install make git build-essential libprotobuf-dev protobuf-compiler libexpat1-dev cmake ruby-ronn libgeos++-dev` # install the required dependencies
* `git clone https://github.com/rbuch703/coords.git` # clone the repository
* `cd coords`
* `cmake .`
* `make` # compile the tools
* `make install` # optional, to install tools and man pages.

For instructions on how to create a COORDS data storage using these tools, and how to render maps, refer to the [COORDS project page](http://rbuch703.github.io/coords).


