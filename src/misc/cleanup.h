
#ifndef MISC_CLEANUP_H
#define MISC_CLEANUP_H

#include <string>

void deleteIfExists(const std::string &storageDirectory, 
                    const std::string &filename);
                    
void deleteNumberedFiles(const std::string &storageDirectory, 
                         const std::string &prefix, 
                         const std::string &suffix);

#endif

