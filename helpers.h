
#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <vector>
//#include <list>
#include "geometric_types.h"

void ensureDirectoryExists(std::string directory);
void createEmptyFile(string file_base);

void dumpPolygon(string file_base, const vector<Vertex>& segment);
double getWallTime();

#endif

