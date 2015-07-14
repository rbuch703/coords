
#ifndef MULTIPOLYGONRECONSTRUCTOR_H
#define MULTIPOLYGONRECONSTRUCTOR_H

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <string>

std::vector<uint64_t> buildMultipolygonGeometry(const std::string &storageDirectory, FILE* fMultipolygonsOut);


#endif

