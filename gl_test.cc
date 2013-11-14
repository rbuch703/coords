
#include <GL/glfw.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include <errno.h>
#include <sys/stat.h>

//#include <malloc.h>


#include <curl/curl.h>

#include <string>
#include <vector>
#include "vertexchain.h"
#include "triangulation.h"
#include "helpers.h"

using namespace std;

typedef struct vertex_data_t
{
    int64_t num_vertices;
    int32_t *vertices;
} vertex_data_t;

int64_t get_x(int32_t* data, int64_t i) { return data[i*2];}
int64_t get_y(int32_t* data, int64_t i) { return data[i*2+1];}
int64_t isClockwise(vertex_data_t polygon)
{
    int32_t *vertex_data = polygon.vertices;
    assert( polygon.num_vertices >= 4);
    assert( get_x(vertex_data, 0) == get_x(vertex_data, polygon.num_vertices-1) );
    assert( get_y(vertex_data, 0) == get_y(vertex_data, polygon.num_vertices-1) );
    
    int64_t n_vertices = polygon.num_vertices-1; //ignore the last vertex, which is a duplicate of the first one

    //int32_t *min_vertex = {vertex_data[0], vertex_data[1]};
    uint32_t min_idx = 0;
    for (int i = 0; i < n_vertices; i++)
        if ( get_x(vertex_data,i) < get_x(vertex_data, min_idx) ||
            ((get_x(vertex_data,i)== get_x( vertex_data, min_idx) ) && 
             (get_y(vertex_data,i) < get_y( vertex_data, min_idx) ) ) )
            min_idx = i;
            
    uint32_t pred_idx = min_idx == 0 ? n_vertices - 1 : min_idx - 1;
    uint32_t succ_idx = min_idx == n_vertices - 1 ? 0 : min_idx + 1;

    assert( (min_idx != pred_idx) && (min_idx != succ_idx) && (pred_idx != succ_idx) );
    int64_t end_x = get_x(vertex_data, succ_idx);
    int64_t end_y = get_y(vertex_data, succ_idx);
    
    int64_t start_x = get_x(vertex_data, pred_idx);
    int64_t start_y = get_y(vertex_data, pred_idx);
    
    int64_t x = get_x( vertex_data, min_idx);
    int64_t y = get_y( vertex_data, min_idx);

    //assert( end_x != x || end_y != y);

    assert( start_x != end_x || start_y != end_y);
    
    int64_t pseudodistance = (end_x - start_x)*(start_y - y) - (end_y - start_y)*(start_x - x);
    //#warning undefined behavior in case of collinear vertices
    assert ( pseudodistance != 0 && "colinear vertices, cannot determine orientation");
    
    return pseudodistance;// < 0;
}


class Tile
{
public:
    virtual void render() const = 0;
};


class AreaTile: public Tile
{
public:    
    AreaTile(const string &filename)
    {
        cout << "opening file " << filename << endl;
        
        FILE* f = fopen(("cache/"+filename).c_str(), "rb");
        assert (f != NULL);
        
        int64_t nVertices = 0;
        while (fread(&nVertices, sizeof(nVertices), 1, f))
        {
        
            int32_t* polygon = new int32_t[2 * nVertices];
        
            if (1 != fread(polygon, nVertices*8, 1, f))
                assert( 0 && "error reading file");
            
            VertexChain c(polygon, nVertices);
            triangulate(c, triangles);
            /*
            if (tile.num_polygons == tile.num_max_polygons)
            {
                tile.polygons = realloc(tile.polygons, 2 * tile.num_max_polygons * sizeof(vertex_data_t));
                tile.num_max_polygons *= 2;
            }

            tile.polygons[tile.num_polygons++] = polygon;        */
        }
        fclose(f);
        cout << "loaded " << filename << " with " << triangles.size()/6 << " triangles." << endl;
    }
    
    virtual void render() const
    {

        //glColor3f(1,1,1);
        //BOOST_FOREACH( CountVertexPair p, polygons)
        assert(triangles.size() % 6 == 0);
        for (uint64_t i = 0; i < triangles.size(); i+=6)
        {
            glBegin(GL_TRIANGLES);
                glVertex2d(triangles[i+1], triangles[i+0]);
                glVertex2d(triangles[i+3], triangles[i+2]);
                glVertex2d(triangles[i+5], triangles[i+4]);
            glEnd();
        }
    }    
private:
    vector<int32_t> triangles;
};

class OutlineTile: public Tile
{
public:
    OutlineTile(const string &filename):
        polygons(new vertex_data_t[16]), num_polygons(0), num_max_polygons(16)
    {
        
        cout << "opening outline file " << filename << endl;
        
        FILE* f = fopen(("cache/"+filename).c_str(), "rb");
        assert (f != NULL);
        
        int64_t nVertices = 0;
        while (fread(&nVertices, sizeof(nVertices), 1, f))
        {
            vertex_data_t vertices;
            vertices.num_vertices = nVertices;
            vertices.vertices = new int32_t[2 * nVertices];
        
            if (1 != fread(vertices.vertices, nVertices*8, 1, f))
                assert( 0 && "error reading file");
            
            //VertexChain c(polygon, nVertices);
            if (num_polygons == num_max_polygons)
            {
                vertex_data_t *poly_tmp = new vertex_data_t[2 * num_max_polygons];
                num_max_polygons *= 2;
                for (int i = 0; i < num_polygons; i++)
                    poly_tmp[i] = polygons[i];
                delete [] polygons;
                polygons = poly_tmp;
            }

            polygons[num_polygons++] = vertices;
        }
        fclose(f);
        cout << "loaded " << filename << endl;//" with " << tile.triangles.size()/6 << " triangles." << endl;
    }
    
    virtual void render() const
    {
        for (int i = 0; i < num_polygons; i++)
        {
               /* int64_t pd = isClockwise(tile.polygons[i]);
                if (pd > 0)
                    glColor3f(1,1,1);
                else if (pd < 0)
                    glColor3f(0,0,0);
                else
                    glColor3f(1,0,0);
                */
            vertex_data_t &poly = polygons[i];
            glBegin(  GL_LINE_LOOP);
                for (int j = 0; j < poly.num_vertices; j++)
                    glVertex2d( poly.vertices[2*j+1], poly.vertices[2*j]);
            glEnd();
        }
    }
    
private:
    vertex_data_t *polygons;
    int64_t num_polygons;
    int64_t num_max_polygons;

};

map<string, Tile*> tileCache;

int button_state[] = {0, 0, 0};
double g_top =    900000000;
double g_bottom =-900000000;
double g_left = -1800000000;
double g_right = 1800000000;

int mouse_x, mouse_y;

void buttonPressed(int button_id, int state)
{
    if (button_id < 0 || button_id > 2) return;
    button_state[button_id] = (state == GLFW_PRESS);
    glfwGetMousePos( &mouse_x, &mouse_y);
}

void mouseMoved(int x, int y)
{

    int dx = x - mouse_x;
    int dy = y - mouse_y;

    if (button_state[0] && !button_state[1] && !button_state[2])
    {
        double pixel_x = (g_bottom-g_top)/ 640;
        double pixel_y = (g_right - g_left)/ 1280;
        
        g_top    += pixel_y * dy;
        g_bottom += pixel_y * dy;
        g_left   += pixel_x * dx;
        g_right  += pixel_x * dx;
    }
    else if (!button_state[0] && button_state[1] && !button_state[2])
    {
        double center_x = (g_right + g_left) / 2.0;
        double center_y = (g_bottom + g_top) / 2.0;
        double half_width = (g_right - g_left)/2.0;
        double half_height = (g_bottom - g_top)/2.0;
        
        double fac = pow( 1.02, y - mouse_y);
        half_width *= fac;
        half_height *= fac;
        
        g_top    = center_y - half_height;
        g_bottom = center_y + half_height;
        g_left   = center_x - half_width;
        g_right  = center_x + half_width;
        
    }
    
    mouse_x = x;
    mouse_y = y;
}


CURL* easyhandle; 

set<string> existing_tiles;

void ensureExistsInCache(const string filename)
{
    struct stat dummy;
    if (stat( ("cache/"+filename).c_str(), &dummy ) == 0) return; //already in cache, nothing to do
    
    // download it to fill the cache (or get the server answer that it does not exist)
        
    curl_easy_setopt(easyhandle, CURLOPT_URL, ("http://91.250.98.219/data/"+filename).c_str() );
    curl_easy_setopt(easyhandle, CURLOPT_USERAGENT, "OSM gl_test-0.1 (libcurl)");
    curl_easy_setopt(easyhandle, CURLOPT_FAILONERROR, true);
    //curl_easy_setopt(easyhandle, CURLOPT_HEADER, 1);

    FILE* f = fopen(("cache/"+filename).c_str(), "wb");
    assert(f && "Could not create file");
    curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, f);
    int res = curl_easy_perform(easyhandle);
    fclose(f);
    
    long http_code = 0;
    #ifndef NDEBUG
        assert(CURLE_OK == curl_easy_getinfo (easyhandle, CURLINFO_RESPONSE_CODE, &http_code));
    #else
        curl_easy_getinfo (easyhandle, CURLINFO_RESPONSE_CODE, &http_code);
    #endif
    
    bool dlOk = res == CURLE_OK && http_code == 200;
    if (!dlOk)
    {
        cout << "[ERR] error downloading " << filename << "; reason: "<< res << "/" << http_code << ")" << endl;
        int res = remove ( ("cache/"+filename).c_str() );
        assert(res = 0);
        assert(false);
    } else
    {
        double speed;
        curl_easy_getinfo (easyhandle, CURLINFO_SPEED_DOWNLOAD, &speed);
        cout << "[INFO] download of " << filename << " completed with " <<  (speed/1024) << "kB/s" << endl;
    }
}

bool tileExists(const string tile_name)
{

    struct stat dummy;

    if (stat( ("cache/"+tile_name).c_str(), &dummy ) == 0) return true; //already in cache
    
    if ( existing_tiles.count(tile_name) == 0) return false; //does not exist on server
    
    ensureExistsInCache(tile_name);
    
    assert( 0 == stat( ("cache/"+tile_name).c_str(), &dummy ));
    return true;
    
}

void render_path(const string &filename, bool asOutline)
{
    if (!tileCache.count(filename))
    
        tileCache.insert( pair<string, Tile*>(filename, 
            asOutline ? (Tile*)(new OutlineTile(filename)) : (Tile*)new AreaTile(filename)  ) );

    assert(tileCache.count(filename));

    tileCache[filename]->render();
}

typedef struct rect_t
{
    double top, left, bottom, right;
} rect_t;

double max(double a, double b) { return a> b ? a : b;}
double min(double a, double b) { return a< b ? a : b;}
double width(rect_t rect) { return rect.right - rect.left;}

void render(const string &path, const rect_t view, rect_t tile, bool asOutline)
{
    if (!tileExists(path)) return;

    assert (
        (max( view.left, tile.left) <= min( view.right, tile.right)) && 
        (max( view.bottom, tile.bottom) <= min( view.top, tile.top) ));
    
    /* if no child exists, then this tile is the highest resolution tile available 
     * at least one child exists, then the the existing tiles provide higher resolution for the respective areas.
     * Since in some areas, no data may to be drawn at all, some children may not exist */
    bool hasChildren = 0;

    hasChildren |= existing_tiles.count(path+"0");
    hasChildren |= existing_tiles.count(path+"1");
    hasChildren |= existing_tiles.count(path+"2");
    hasChildren |= existing_tiles.count(path+"3");
    
    int sufficientResolution = width(tile) < 2*width(view);
    if (!hasChildren || sufficientResolution) 
    {
        //cout << "rendering tile " << position << endl;
        render_path(path, asOutline);
        //free(path);
        //Tile(BASEPATH+position).render(); 
        return;
    }
    
    double mid_x = (tile.right+tile.left) / 2.0;
    double mid_y = (tile.top + tile.bottom) / 2.0;
    
    rect_t tl2 = {tile.top, tile.left, mid_y,       mid_x};
    rect_t bl0 = {mid_y,    tile.left, tile.bottom, mid_x};
    rect_t tr3 = {tile.top, mid_x,     mid_y,       tile.right};
    rect_t br1 = {mid_y,    mid_x,     tile.bottom, tile.right};

    if ( mid_x > view.left) //has to render left half
    {
        if (mid_y > view.bottom) render(path+'0', view, bl0, asOutline);
        if (mid_y < view.top   ) render(path+'2', view, tl2, asOutline);
    }
    
    if ( mid_x < view.right) //has to render right half
    {
        if (mid_y > view.bottom) render(path+'1', view, br1, asOutline);
        if (mid_y < view.top   ) render(path+'3', view, tr3, asOutline);
    }
}

void registerTile( string name, FILE* f, set<string> &tiles_out)
{
    uint8_t children;
    fread(&children, sizeof(children), 1, f);
    
    if (children & 1) tiles_out.insert(name+"0");
    if (children & 1) registerTile(name+"0", f, tiles_out);

    if (children & 2) tiles_out.insert(name+"1");
    if (children & 2) registerTile(name+"1", f, tiles_out);

    if (children & 4) tiles_out.insert(name+"2");
    if (children & 4) registerTile(name+"2", f, tiles_out);

    if (children & 8) tiles_out.insert(name+"3");
    if (children & 8) registerTile(name+"3", f, tiles_out);
}

void registerTiles( string indexfile, string basename, set<string> &tiles_out)
{
    FILE* f = fopen(indexfile.c_str(), "rb");
    tiles_out.insert( basename ); //existence of root file is implied and not stored in the index
    
    registerTile( basename, f, tiles_out);
    fclose(f);
}

//static const char* BASEPATH = "output/coast/seg#";
void onResize( int width, int height )
{

    glViewport(0,0,width, height);
    double y_mid = (g_top + g_bottom) / 2.0;
    double myHeight = (g_right - g_left) / width * height;
    g_top   = y_mid + myHeight/2.0;
    g_bottom= y_mid - myHeight/2.0;
}

int main () {
    int running = 1;

    curl_global_init(CURL_GLOBAL_ALL);
    easyhandle = curl_easy_init(); 
    ensureDirectoryExists("cache");
    
    
    ensureExistsInCache("seg.idx");
    ensureExistsInCache("country.idx");
    ensureExistsInCache("building.idx");
    ensureExistsInCache("state.idx");
    
    registerTiles( "cache/seg.idx",     "seg",     existing_tiles);
    registerTiles( "cache/country.idx", "country", existing_tiles);
    registerTiles( "cache/building.idx","building",existing_tiles);
    registerTiles( "cache/state.idx",   "state",   existing_tiles);
/*    Tile t("output/coast/seg#1");
    Tile t0("output/coast/seg#3");
    Tile t2("output/coast/seg#2");*/
    // Initialize GLFW
    glfwInit ();

    // Open an OpenGL window (you can also try Mode.FULLSCREEN)
    if (!glfwOpenWindow (1280, 640, 0, 0, 0, 0, 32, 0, GLFW_WINDOW)) {
        glfwTerminate ();
        return 1;
    }
    
    glfwSetMouseButtonCallback( buttonPressed );
    glfwSetMousePosCallback( mouseMoved );
    glfwSetWindowSizeCallback( onResize);
    // Main loop
    while (running) {
        
        // OpenGL rendering goes here...
        glClearColor(0.58f, 0.66f, 0.78f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(g_left, g_right, g_bottom, g_top, 0, 10);

        
        rect_t view ={ g_top, g_left, g_bottom, g_right };
        rect_t world={ 900000000, -1800000000, -900000000, 1800000000};
        glColor3f(1,1,1);
        render( "seg", view, world, false);
        glColor3f(0,1,0);
        render( "state", view, world, true);
        glColor3f(0,0,0);
        render( "country", view, world, true);
        glColor3f(1,0,0);
        render( "building", view, world, false);

        // Swap front and back rendering buffers
        glfwSwapBuffers ();
        // Check if ESC key was pressed or window was closed
        running = !glfwGetKey (GLFW_KEY_ESC) && glfwGetWindowParam (GLFW_OPENED);
    }

    // Close window and terminate GLFW
    glfwTerminate ();
    curl_global_cleanup();
    return 0;
}

