
#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <list>
#include "geometric_types.h"

void ensureDirectoryExists(std::string directory);
void dumpPolygon(string file_base, const list<Vertex>& segment);
double getWallTime();

#endif

