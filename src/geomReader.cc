
#include <stdio.h>
#include <iostream>
#include "geom/genericGeometry.h"
#include "osm/osmBaseTypes.h"
#include "config.h"

int main()
{
    FILE* f = fopen("intermediate/multipolygons.bin", "rb");
    MUST(f, "cannot open file");
    int ch;
    while ( (ch = fgetc(f)) != EOF)
    {
        ungetc(ch, f);
        GenericGeometry geom(f);
                  
        //Envelope env = geom.getBounds();
        if (geom.numBytes > 500000)
            std::cout << geom.getEntityType() << " " << geom.getEntityId()
                      << " has " << (geom.numBytes/1000000.0) << "MB" << std::endl;
                  
/*
        std::cout << " lat: " << (env.latMin / 10000000.0) << "-> " << (env.latMax / 10000000.0);
        std::cout << ", ";
        std::cout << "lng: " << (env.lngMin / 10000000.0) << "-> " << (env.lngMax / 10000000.0);
        std::cout << std::endl;*/
    }

    return EXIT_SUCCESS;
}
