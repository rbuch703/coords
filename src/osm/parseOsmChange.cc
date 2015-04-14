
#include <stdio.h>
#include <expat.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h> //for strcmp
#include <map>
#include <vector>
#include <iostream>

#include "osmTypes.h"
#define BUFFSIZE	8192

using std::map;
using std::vector;
using std::string;
using std::make_pair;
using std::cout;
using std::endl;
char Buff[BUFFSIZE];

//int Depth;

enum CHANGE_TYPE {CT_UNDEFINED, CREATE, UPDATE, DELETE};
enum ENTITY_TYPE {ET_UNDEFINED, ET_NODE, ET_WAY, ET_RELATION};

typedef map<string, string> Attributes;

typedef struct ParserState {
    bool changeHeaderParsed;
    CHANGE_TYPE changeType;
    ENTITY_TYPE entityType;
    Attributes  entityTags;
    int64_t     entityId;
    uint64_t    entityVersion;
    double      entityLat;
    double      entityLng;
    vector<OsmRelationMember> entityMembers;
    vector<int64_t> entityRefs;
    int depth;
    
} ParserState;


Attributes getDict(const char **attr)
{
    map<string, string> res;
    for (int i = 0; attr[i]; i+=2)
        res.insert(make_pair( attr[i], attr[i+1]));
    return res;
}

void start(ParserState *state, const char *el, const char **attr) {

    if (strcmp(el, "osmChange") == 0)
    {
        assert(!state->changeHeaderParsed);
        state->changeHeaderParsed = true;
        Attributes attributes = getDict(attr);
        assert( attributes.count("version") && attributes.count("generator") && "Missing osmChange attributes");
        assert( attributes["version"] == "0.6" && "unsupported file format version (only version 0.6 is supported");
        cout << "File format version is '" << attributes["version"] << "', created by '" << attributes["generator"] << "'" << endl;
        return;
    } 
    
    if (strcmp(el, "modify") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType == CT_UNDEFINED);
        state->changeType = UPDATE;
        //cout << "[DBG ] change mode is now UPDATE" << endl;
        return;
    }
    
    if (strcmp(el, "delete") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType == CT_UNDEFINED);
        state->changeType = DELETE;
        //cout << "[DBG ] change mode is now DELETE" << endl;
        return;
    }

    if (strcmp(el, "create") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType == CT_UNDEFINED);
        state->changeType = CREATE;
        //cout << "[DBG ] change mode is now CREATE" << endl;
        return;
    }

    
    if (strcmp(el, "node") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType != CT_UNDEFINED);
        assert(state->entityType == ET_UNDEFINED);
        assert(state->entityTags.size() == 0);
        assert(state->entityMembers.size() == 0);
        assert(state->entityRefs.size() == 0);

        state->entityType = ET_NODE;
        Attributes attributes = getDict(attr);
        assert( attributes.count("id") && 
                attributes.count("version") &&
                attributes.count("lat") && 
                attributes.count("lon") && "Missing node attributes");
        state->entityId      = atoll( attributes["id"].c_str());
        if (state->entityId < 0)
        {
            cout << "[ERR ]\e[31m invalid node id " << state->entityId << "\e[39m" << endl;
            exit(0);
        }
        state->entityVersion = atoll( attributes["version"].c_str());
        state->entityLat     = atof( attributes["lat"].c_str());
        state->entityLng     = atof( attributes["lon"].c_str());
        
        //cout << "\e[2m[DBG ] node " << state->entityId  << " v." << state->entityVersion << ": " << state->entityLat << "/" << state->entityLng << "\e[22m" << endl;
        
        return;
    }

    if (strcmp(el, "way") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType != CT_UNDEFINED);
        assert(state->entityType == ET_UNDEFINED);
        assert(state->entityTags.size() == 0);
        assert(state->entityMembers.size() == 0);
        assert(state->entityRefs.size() == 0);
        
        state->entityType = ET_WAY;
        Attributes attributes = getDict(attr);
        assert( attributes.count("id") && 
                attributes.count("version") && "Missing way attributes");

        state->entityId      = atoll( attributes["id"].c_str());
        if (state->entityId < 0)
        {
            cout << "[ERR ]\e[31m invalid way id " << state->entityId << "\e[39m" << endl;
            exit(0);
        }
        state->entityVersion = atoll( attributes["version"].c_str());
        //cout << "\e[2m[DBG ] way " << state->entityId  << " v." << state->entityVersion << "\e[22m" << endl;
        return;
    }

    if (strcmp(el, "relation") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType != CT_UNDEFINED);
        assert(state->entityType == ET_UNDEFINED);
        assert(state->entityTags.size() == 0);
        assert(state->entityMembers.size() == 0);
        assert(state->entityRefs.size() == 0);
        
        state->entityType = ET_RELATION;
        Attributes attributes = getDict(attr);
        assert( attributes.count("id") && 
                attributes.count("version") && "Missing relation attributes");

        state->entityId      = atoll( attributes["id"].c_str());
        if (state->entityId < 0)
        {
            cout << "[ERR ]\e[31m invalid relation id " << state->entityId << "\e[39m" << endl;
            exit(0);
        }
        state->entityVersion = atoll( attributes["version"].c_str());
        //cout << "\e[2m[DBG ] relation " << state->entityId  << " v." << state->entityVersion << "\e[22m" << endl;
        return;
    }

    
    if (strcmp(el, "tag") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType != CT_UNDEFINED);
        assert(state->entityType != ET_UNDEFINED);
        assert( attr[0] && attr[1] && attr[2] && attr[3] && !attr[4]); //has exactly 2x2 kv entries
        assert( strcmp(attr[0],"k") == 0 && strcmp(attr[2],"v") == 0); //k=X v=Y
        const char* key = attr[1];
        const char* value=attr[3];
        state->entityTags.insert(make_pair(key, value));
        //cout << "\e[2m[DBG ]     " << key << "=" << value << "\e[22m" << endl;
        return;
    }

    if (strcmp(el, "nd") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType != CT_UNDEFINED);
        assert(state->entityType == ET_WAY);
        assert(attr[0] && attr[1] && !attr[2]); //has exactly 1x2 kv entries (ref=X)
        assert(strcmp(attr[0], "ref") == 0);
        int64_t refId = atoll(attr[1]);
        //cout << "\e[2m[DBG ]     ref -> #" << refId << "\e[22m" << endl;
        state->entityRefs.push_back( refId );
        return;
    }

    if (strcmp(el, "member") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType != CT_UNDEFINED);
        assert(state->entityType == ET_RELATION);
        assert(attr[0] && attr[1] && attr[2] && attr[3] && attr[4] && attr[5] && !attr[6]); //has exactly 3x2 kv entries (type=X, ref=Y, role=Z)
        assert(strcmp(attr[0], "type") == 0);
        assert(strcmp(attr[2], "ref") == 0);
        assert(strcmp(attr[4], "role") == 0);
        assert( (strcmp(attr[1], "node") == 0) | 
                (strcmp(attr[1], "way" ) == 0) | 
                (strcmp(attr[1], "relation") == 0));
                
        ELEMENT type = strcmp(attr[1], "node")==0 ? NODE :
                       strcmp(attr[1], "way") ==0 ? WAY : RELATION;
        const char* role = attr[5];
        int64_t refId = atoll(attr[3]);
        //cout << "\e[2m[DBG ]     ref -> " << attr[1] << " #" << refId << " (" << role << ")\e[22m" << endl;
        state->entityMembers.push_back( OsmRelationMember(type, refId, role) );
        return;
    }


    for (int i = 0; i < state->depth; i++)
        printf("  ");

    cout << "[WARN] \e[93munknown xml entity '"<< el << "'\e[39m" << endl;
    exit(0);

    for (int i = 0; attr[i]; i += 2) {
        printf(" %s='%s'", attr[i], attr[i + 1]);
    }

    printf("\n");
    state->depth += 1;
}  /* End of start handler */


int nNodes, nWays, nRelations;

void end(ParserState *state, const char *el) {

    if (strcmp(el, "node") == 0)
    {
        assert(state->entityType == ET_NODE);

        
        //FIXME: add code for completing nodes
        /*
        switch( state->changeType) {
            case CREATE: assert(false && "Not implemented"); break;
            case UPDATE: assert(false && "Not implemented"); break;
            case DELETE: assert(false && "Not implemented"); break;
            case CT_UNDEFINED: assert(false); break;
        }*/
        nNodes += 1;
        state->entityTags.clear();        
        state->entityType = ET_UNDEFINED;
        return;
    }

    if (strcmp(el, "way") == 0)
    {
        assert(state->entityType == ET_WAY);

        
        //FIXME: add code for completing ways
        /*
        switch( state->changeType) {
            case CREATE: assert(false && "Not implemented"); break;
            case UPDATE: assert(false && "Not implemented"); break;
            case DELETE: assert(false && "Not implemented"); break;
            case CT_UNDEFINED: assert(false); break;
        }*/
        nWays += 1;
        state->entityRefs.clear();
        state->entityTags.clear();
        state->entityType = ET_UNDEFINED;
        return;
    }

    if (strcmp(el, "relation") == 0)
    {
        assert(state->entityType == ET_RELATION);

        
        //FIXME: add code for completing relations
        /*
        switch( state->changeType) {
            case CREATE: assert(false && "Not implemented"); break;
            case UPDATE: assert(false && "Not implemented"); break;
            case DELETE: assert(false && "Not implemented"); break;
            case CT_UNDEFINED: assert(false); break;
        }*/
        nRelations += 1;
        state->entityTags.clear();
        state->entityMembers.clear();
        state->entityType = ET_UNDEFINED;
        return;
    }

    
    if (strcmp(el, "tag") == 0)
    {
        assert( state->entityType != ET_UNDEFINED);
        return;
    }

    if (strcmp(el, "nd") == 0)
    {
        assert( state->entityType == ET_WAY);
        return;
    }

    if (strcmp(el, "member") == 0)
    {
        assert( state->entityType == ET_RELATION);
        return;
    }


    if (strcmp(el, "create") == 0)
    {
        assert( state->changeType == CREATE);
        state->changeType = CT_UNDEFINED;
        return;
    }


    if (strcmp(el, "modify") == 0)
    {
        assert( state->changeType == UPDATE);
        state->changeType = CT_UNDEFINED;
        return;
    }

    if (strcmp(el, "delete") == 0)
    {
        assert( state->changeType == DELETE);
        state->changeType = CT_UNDEFINED;
        return;
    }

    if (strcmp(el, "osmChange") == 0)
    {
        assert(state->changeHeaderParsed);
        assert(state->changeType == CT_UNDEFINED);
        assert(state->entityType == ET_UNDEFINED);
        state->changeHeaderParsed = false;
        cout << "done." << endl;
        return;
    }

    cout << "end of unknown entity '" << el << "'" << endl;


    state->depth -= 1;
}  /* End of end handler */

int main(int, char **) {

    cout << "\e[0m"; //reset all attributes
    
    nNodes = nWays = nRelations = 0;
    
    ParserState state{ .changeHeaderParsed = 0, .changeType=CT_UNDEFINED, .entityType = ET_UNDEFINED, .entityTags = Attributes(), .entityId = 0, .entityVersion = 0, .entityLat = 0.0, .entityLng = 0.0, .entityMembers = vector<OsmRelationMember>(), .entityRefs = vector<int64_t>(),
.depth=0};
    
    XML_Parser p = XML_ParserCreate(NULL);
    if (! p) {
        fprintf(stderr, "Couldn't allocate memory for parser\n");
        exit(-1);
    }
    
    
    XML_SetUserData(p, &state);

    XML_SetElementHandler(p, (XML_StartElementHandler)start, (XML_EndElementHandler)end);

    while (!feof(stdin))
    {
        int len = fread(Buff, 1, BUFFSIZE, stdin);
        if (ferror(stdin)) {
            fprintf(stderr, "Read error\n");
            exit(-1);
        }

        if (! XML_Parse(p, Buff, len, len < BUFFSIZE)) {
            fprintf(stderr, "Parse error at line %" PRIu64 ":\n%s\n",
            XML_GetCurrentLineNumber(p),
            XML_ErrorString(XML_GetErrorCode(p)));
            exit(-1);
        }
    }

    XML_ParserFree(p);
    cout << "stats: nodes/ways/relations: " << nNodes << "/" << nWays << "/" << nRelations << endl;
    return EXIT_SUCCESS;
}  

