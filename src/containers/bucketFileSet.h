
#ifndef BUCKETFILESET_H
#define BUCKETFILESET_H

#include <string>
#include <vector>

/* The BucketFileSet stores a (potentially huge) set of key-value pairs on disk.
   The keys have to be of type uint64_t, while the values can be of arbitrary type.
   A BucketFileSet does not store all data in a single file, but rather partitions
   the set of key-value pairs into several files. Each file holds all key-value 
   pairs of a key range of size 'bucketSize' (e.g. file 0 holds all pairs with keys
   in [0 .. bucketSize-1], file 1 holds all pairs with keys in [bucketSize, 2*BucketSize-1]
   and so on.
   
   A BucketFileSet is not intended for quick data retrieval (and in fact contains no
   methods to directly access any key-value pair. It is just a way of conveniently
   subdividing an unmanageably large data set into buckets of managable size.
 
 */
template<typename ValueType>
class BucketFileSet
{
public:
    BucketFileSet(std::string baseName, uint64_t bucketSize, bool appendToExistingFiles);    
    ~BucketFileSet();
    
    FILE*    getFile(uint64_t id);
    void     clearBucket(uint64_t bucketId);
    void     clear();
    void     write(uint64_t id, const ValueType& data);
    uint64_t getNumBuckets() const;
    
    std::vector< std::pair<uint64_t, ValueType>> getContents(uint64_t bucketId);

private:
    static std::string toBucketString(std::string baseName, uint64_t bucketId);
    void addBuckets(uint64_t newMaxBucketId);

private:
    std::vector<FILE*>  bucketFiles;
    std::string         baseName;
    uint64_t            bucketSize;
};


//===================================================
#define __STDC_FORMAT_MACROS
#include <inttypes.h>   //for PRIu64
#include <stdio.h> //for FILE*
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>     //for ftruncate

template<typename ValueType>
BucketFileSet<ValueType>::BucketFileSet(std::string baseName, uint64_t bucketSize, bool appendToExistingFiles):
        baseName(baseName), bucketSize(bucketSize)
{
    if (!appendToExistingFiles)
    {
        unlink( toBucketString(baseName, 0).c_str() );
        return;
    }

    struct stat dummy;
    std::string fileName;
    
    for (uint64_t id = 0 ; 
         stat((fileName = toBucketString(baseName, id)).c_str(), &dummy) == 0; 
         id++)
    {
        FILE* f = fopen(fileName.c_str(), "a+b");
        if (!f)
            perror("fopen");
        MUST(f, "cannot open bucket file");
        bucketFiles.push_back(f);
    }
}

template<typename ValueType>
BucketFileSet<ValueType>::~BucketFileSet() 
{
    for (FILE* f : bucketFiles)
        fclose(f);
}


template<typename ValueType>
FILE* BucketFileSet<ValueType>::getFile(uint64_t id)
{
    uint64_t bucketId = id / this->bucketSize;
    assert( bucketId < 800 && "getting too close to OS ulimit for open files");
    if (bucketId >= bucketFiles.size() )
        addBuckets(bucketId);

    return bucketFiles[bucketId];
}

template<typename ValueType>
void BucketFileSet<ValueType>::clearBucket(uint64_t bucketId) 
{
    if (bucketId >= bucketFiles.size())
        return; //nothing to clear
        
    MUST(ftruncate( fileno(bucketFiles[bucketId]), 0) == 0, "could not truncate bucket file");
}

template<typename ValueType>
void BucketFileSet<ValueType>::clear()
{
    for (uint64_t i = 0; i < bucketFiles.size(); i++)
    {
        fclose(bucketFiles[i]);
        unlink( toBucketString(baseName, i).c_str());
    }
    
    bucketFiles.clear();
}

template<typename ValueType>
void BucketFileSet<ValueType>::write(uint64_t id, const ValueType& data)
{
    uint64_t bucketId = id / bucketSize;
    assert( bucketId < 800 && "getting too close to OS ulimit for open files");
    if (bucketId >= bucketFiles.size() )
        addBuckets(bucketId);

    MUST( fwrite( &id, sizeof(id), 1, bucketFiles[bucketId]) == 1, "bucket write failed");
    MUST( fwrite( &data, sizeof(data),  1, bucketFiles[bucketId]) == 1, "bucket write failed");
}

template<typename ValueType>
uint64_t BucketFileSet<ValueType>::getNumBuckets() const 
{ 
    return bucketFiles.size();
}

template<typename ValueType>
std::vector< std::pair<uint64_t, ValueType>> BucketFileSet<ValueType>::getContents(uint64_t bucketId)
{
    MUST(bucketId < bucketFiles.size(), "Bucket index out of bounds.");
    FILE* &f = bucketFiles[bucketId];

    fseek(f, 0, SEEK_END);
    uint64_t fileSize = ftell(f);
    uint64_t kvSize = sizeof(uint64_t) + sizeof(ValueType);
    MUST(fileSize % kvSize == 0, "bucket file corruption");
    uint64_t numItems = fileSize / kvSize;
    std::vector< std::pair<uint64_t, ValueType>> res;
    res.reserve(numItems);
    
    uint8_t *rawData = new uint8_t[fileSize];
    fseek(f, 0, SEEK_SET); //seek to beginning for reading
    // this fread() also re-positions the file pointer to the end of the file.
    // This is necessary for later write() calls to no corrupt the data.
    MUST( fread( rawData, fileSize, 1, f) == 1, "error reading bucket file");
    
    uint8_t *rawDataPos = rawData;
    while (numItems--)
    {
        uint64_t key = *(uint64_t*)rawDataPos;
        rawDataPos += sizeof(uint64_t);
        ValueType value = *(ValueType*)rawDataPos;
        rawDataPos += sizeof(ValueType);
        res.push_back(std::make_pair(key, value));
    }
    
    delete [] rawData;
    return res;
    
}

template<typename ValueType>
std::string BucketFileSet<ValueType>::toBucketString(std::string baseName, uint64_t bucketId)
{
    char tmp [100];
    MUST( snprintf(tmp, 100, "%05" PRIu64, bucketId) < 100, "bucket id overflow");
    /* warning: do not attempt to integrate the following concatenation
     *          into the sprintf statement above. 'baseName' has
     *          unbounded length, and thus may overflow any preallocated
     *          char array.*/
    return baseName + tmp + ".raw";
}

template<typename ValueType>
void BucketFileSet<ValueType>::addBuckets(uint64_t newMaxBucketId)
{
    uint64_t oldNumBuckets = bucketFiles.size();
    bucketFiles.resize( newMaxBucketId + 1, nullptr);
    
    for (uint64_t i = oldNumBuckets; i < bucketFiles.size(); i++)
    {
        /* always truncate the new bucket file ("wb"), even if we are appending
         * to the bucket set ('appendToExistingFiles'): in append mode, all
         * existing bucket files belonging to the current set have already been
         * opened through the BucketFileSet constructor. Any additional existing
         * bucket files do not belong to this set, but are leftovers from previous
         * program executions */
        std::string filename = toBucketString(baseName, i);
        FILE* f = fopen( filename.c_str(), "wb");
        MUST(f, "cannot create bucket file");
        bucketFiles[i] = f;
    }
        
    /* try to delete the bucket file *after* the last one. 
     * Usually, this one should not exist, and unlink() will just fail gracefully.
     * But in case there are leftover bucket files from an earlier run, this will
     * ensure that there is no unbroken sequence of files that contains both old 
     * and new bucket files. Thus, this deletion ensures that subsequent tools do not
     * use data from earlier runs.
     */
    unlink( toBucketString(baseName, bucketFiles.size() ).c_str()); 
}


#endif

