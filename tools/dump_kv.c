
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


const char* getValue(const char* line, const char* key)
{
    static char* buffer = NULL;
    static uint32_t buffer_size = 0;

    uint32_t len = strlen(key)+3;
    if (buffer_size < len)
    {
        if (buffer) free(buffer);
        buffer = (char*)malloc(len);
        buffer_size = strlen(key)+3;
    }
    strcpy(buffer, key);
    buffer[ len-3] = '=';
    buffer[ len-2] = '"';
    buffer[ len-1] = '\0';
    
    const char* in = strstr(line, buffer);
    assert (in != NULL);
    in += (strlen(buffer)); //skip key + '="'
    const char* out = strstr(in, "\"");
    assert(out != NULL);

    uint32_t size = out - in;
    if (buffer_size <= size)
    {
        if (buffer) free(buffer);
        buffer = (char*)malloc(size+1);
        buffer_size = size+1;
    }
    memcpy(buffer, in, size);
    buffer[size] ='\0';
    //std::cout << buffer << std::endl;
    return buffer;
}

int main()
{

        
    int in_fd = 0; //default to standard input
    //in_fd = open("isle_of_man.osm", O_RDONLY);
    FILE * in_file = fdopen(in_fd, "r"); 

    fseek( in_file, 0, SEEK_END );
    uint64_t file_size = ftello(in_file);
    fseek(in_file, 0, SEEK_SET);
    
    char* line_buffer = NULL;
    size_t line_buffer_size = 0;

    
    /*ofstream of_kv;
    of_kv.open("kv_dump.csv");*/
    FILE* out_file = fopen("kv_dump.c.csv", "wb");
    
    uint64_t num_lines = 0;
    char *key = malloc(1000);
    uint32_t key_size = 1000;
    
    while (0 <= getline(&line_buffer, &line_buffer_size, in_file))
    {   
        num_lines++;
        if (num_lines % 1000000 == 0)
        {
            uint64_t pos = ftello(in_file);
            printf("%f %% (%d M lines)\n", pos / (double)file_size* 100, (int)(num_lines / 1000000));
        }
        if (! strstr(line_buffer, "<tag") ) continue;
        
        // getValue returns a pointer to its internal buffer. So we need to copy its result
        // before calling getValue() again;
        const char* tmp = getValue(line_buffer, "k");
        uint32_t tmp_len = strlen(tmp)+1;
        if (key_size < tmp_len)
        {
            free(key);
            key = malloc(tmp_len);
            key_size = tmp_len;
        }
        strcpy(key, tmp);
        
        fprintf(out_file, "%s§%s\n", key, getValue(line_buffer, "v") );
        /*std::string key = getValue(line_buffer, "k");
        std::string val = getValue(line_buffer, "v");
        of_kv << key << "§" << val << std::endl;*/
    } 
    free (line_buffer);
    free(key);
    fclose(in_file);
    fclose(out_file);
//    of_kv.close();    

    //std::cout << "#key-value pairs: " << kv_map.size() << std::endl;
	return 0;
	
}


