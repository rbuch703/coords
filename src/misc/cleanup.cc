
#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "misc/escapeSequences.h"

void deleteIfExists(const std::string &storageDirectory, const std::string &filename)
{
    std::string file = storageDirectory + filename;
    int res = unlink( file.c_str());
    
    if (res == 0)
        return;
        
    switch (errno)
    {
        case EACCES:
            std::cerr << ESC_FG_RED << "[ERR ] could not delete file '" << file 
                      << "': access denied." << ESC_RESET << std::endl;
            exit(0);
            return;
        
        case ENOENT:
            // file did not exist --> nothing to do
            /*std::cerr << ESC_FG_GRAY << "file '" << file << "' did not exist." 
                      << ESC_RESET << std::endl;*/
            return;
        default:
            std::cerr << ESC_FG_RED << "[ERR ] could not delete file '" << file 
                      << "'" << ESC_RESET;
            std::cerr.flush();
            perror("");
            exit(0);
    }
}

void deleteNumberedFiles(const std::string &storageDirectory, const std::string &prefix, const std::string &suffix)
{
    DIR *dp;
    struct dirent *ep;     
    dp = opendir (storageDirectory.c_str());

    if (dp == NULL)
    {
        std::cerr << ESC_FG_RED << "[ERR ] Couldn't open directory '" << storageDirectory 
                  << "':" << ESC_RESET;
                  
        std::cerr.flush();
        perror("");
        exit(0);
    }
    
    while ( (ep = readdir (dp)) )
    {        
        const char* file = ep->d_name;
        //std::cerr << "candidate: " << file << std::endl;
        bool correctPrefix = strlen(file) >= prefix.length() && 
                             (strncmp( file, prefix.c_str(), prefix.length()) == 0);
        if (!correctPrefix)
            continue;

        const char* fileWithoutPrefix = file + prefix.length();
        //std::cerr << "\tcandidate without prefix: " << fileWithoutPrefix << std::endl;
        
        uint64_t sLen = suffix.length();
        if (strlen(fileWithoutPrefix) < sLen)
            continue;

        std::string fileSuffix(fileWithoutPrefix, 
                               strlen(fileWithoutPrefix) - suffix.length(), suffix.length());
        //std::cerr << "\tcandidate suffix: " << fileSuffix << std::endl;
        
        if (fileSuffix != suffix)
            continue;

        std::string fileBase(fileWithoutPrefix, strlen(fileWithoutPrefix) - suffix.length());
        //std::cerr << "\tcandidate base: " << fileBase << std::endl;
            
        bool onlyDigits = true;
        int numDigits = 0;
        
        for ( const char* pos = fileBase.c_str(); *pos != '\0'; pos++)
        {
            numDigits += 1;
            onlyDigits &= (*pos >= '0' && *pos <= '9');
        }
        
        if (!onlyDigits || numDigits < 1)
            continue;
        
        //std::cerr << ESC_FG_GREEN << file << ESC_RESET << std::endl;
        deleteIfExists(storageDirectory, file);
        
  
    }

    (void) closedir (dp);

}



void ensureDirectoryExists(const std::string &directory)
{
    struct stat dummy;
    size_t start_pos = 0;
    
    for ( size_t pos = directory.find('/', start_pos);
          pos != std::string::npos;
          pos = directory.find('/', start_pos) )
    {
        std::string basedir = directory.substr(0, pos);  //works even if no slash is present --> pos == string::npos
        if (0!= stat(basedir.c_str(), &dummy)) //directory does not yet exist
            if (0 != mkdir(basedir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) //755
            { 
                perror(("[ERR] mkdir '" + directory + "'").c_str());
                abort();
            }
                
        start_pos = pos+1;
    }
}

