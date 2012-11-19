
#include <GL/glfw.h>
#include <string>
#include <list>
#include <map>
#include <iostream>
#include <vector>
#include <boost/foreach.hpp>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <errno.h>

#include <sys/stat.h>

#include "geometric_types.h"

using namespace std;

typedef pair<int64_t, int32_t*> CountVertexPair;

class Tile {
    public:
        Tile() {}
        Tile(string filename) 
        {
            FILE* f = fopen(filename.c_str(), "rb");
            if (f == NULL) return;
            int64_t nVertices = 0;
            while (fread(&nVertices, sizeof(nVertices), 1, f))
            {
                int32_t *data = new int32_t[nVertices*2];
                if (1 != fread(data, nVertices*8, 1, f))
                    assert(false && "error reading file");
                
                polygons.push_back( CountVertexPair(nVertices, data));
            }
            fclose(f);
        }
        
        ~Tile()
        {
            BOOST_FOREACH( CountVertexPair p, polygons)
                delete [] p.second;
        }

        void render()
        {
        
            //glColor3f(1,1,1);
            BOOST_FOREACH( CountVertexPair p, polygons)
            {
                glBegin(GL_LINE_STRIP);
                    PolygonSegment ps(p.second, p.first);
                    if (ps.isClockwise())
                        glColor3f(1,1,1);
                    else
                        glColor3f(0,0,0);
                    
                    int32_t* v = p.second;
                    for (int64_t num_vertices = p.first; num_vertices; num_vertices--, v+=2)
                    {
                        glVertex2d(v[1], v[0]);
                    }
                glEnd();
            }
        }
    
    list<CountVertexPair> polygons;
    
};

bool button_state[] = {false, false, false};
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

map<string, Tile> tile_cache;

void renderTile(string filename)
{
    if (!tile_cache.count(filename))
    {
        struct stat dummy;
        if ( stat( filename.c_str(), &dummy ) == 0 )
            tile_cache.insert( pair<string, Tile>(filename, Tile(filename)));
        else
            tile_cache.insert(pair<string, Tile>(filename, Tile())); //insert a dummy to prevent further stat's tothe no-existent file
        cout << "reading file " << filename << endl;
        
        return;
    }
    tile_cache[filename].render();
    
    
}


static const string BASEPATH = "output/coast/seg#";
void render(const AABoundingBox &view, AABoundingBox tile, string position)
{
    struct stat dummy;
    if ( stat( (BASEPATH+position).c_str(), &dummy ) != 0) return;

    assert (
        (max( view.left, tile.left) <= min( view.right, tile.right)) && 
        (max( view.bottom, tile.bottom) <= min( view.top, tile.top) ));
    
    /* if no child exists, then this tile is the highest resolution tile available 
     * at least one child exists, then the the existing tiles provide higher resolution for the respective areas.
     * Since in some areas, no data may to be drawn at all, some children may not exist */
    bool hasChildren = 
        stat( (BASEPATH+position+"0").c_str(), &dummy ) == 0 ||
        stat( (BASEPATH+position+"1").c_str(), &dummy ) == 0 ||
        stat( (BASEPATH+position+"2").c_str(), &dummy ) == 0 ||
        stat( (BASEPATH+position+"3").c_str(), &dummy ) == 0;
        
        
    bool sufficientResolution = tile.width() < view.width();
    if (!hasChildren || sufficientResolution) 
    { 
        //cout << "rendering tile " << position << endl;
        //renderTile(BASEPATH+position);
        Tile(BASEPATH+position).render(); 
        return;
    }
    
    int64_t mid_x = (tile.right+tile.left)   / 2.0;
    int64_t mid_y = (tile.top + tile.bottom) / 2.0;
    
    AABoundingBox tl2(tile.top, tile.left, mid_y,       mid_x);
    AABoundingBox bl0(mid_y,    tile.left, tile.bottom, mid_x);
    AABoundingBox tr3(tile.top, mid_x,     mid_y,       tile.right);
    AABoundingBox br1(mid_y,    mid_x,     tile.bottom, tile.right);
    
    if ( mid_x > view.left) //has to render left half
    {
        if (mid_y > view.bottom) render(view, bl0, position+"0");
        if (mid_y < view.top   ) render(view, tl2, position+"2");
    }
    
    if ( mid_x < view.right) //has to render right half
    {
        if (mid_y > view.bottom) render(view, br1, position+"1");
        if (mid_y < view.top   ) render(view, tr3, position+"3");
    }

}

int main () {
    int running = 1;

    Tile t("output/coast/seg#");
    Tile t0("output/coast/seg#0");
    Tile t2("output/coast/seg#2");
    // Initialize GLFW
    glfwInit ();

    // Open an OpenGL window (you can also try Mode.FULLSCREEN)
    if (!glfwOpenWindow (1280, 640, 0, 0, 0, 0, 32, 0, GLFW_WINDOW)) {
        glfwTerminate ();
        return 1;
    }
    
    glfwSetMouseButtonCallback( buttonPressed );
    glfwSetMousePosCallback( mouseMoved );
    // Main loop
    while (running) {
        
        // OpenGL rendering goes here...
        glClearColor(0.58f, 0.66f, 0.78f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        //glOrtho(-10, 10, -10, 10, 0, 10);
        glOrtho(g_left, g_right, g_bottom, g_top, 0, 10);

        //glColor3f(0,0,0);
        //t.render();
        glColor3f(1,1,1);
        cout << "Cache has stored " << tile_cache.size() << " tiles" << endl;
        render( AABoundingBox(g_top, g_left, g_bottom, g_right), 
                AABoundingBox(900000000, -1800000000, -900000000, 1800000000), "");
        //t0.render();
        //t2.render();
        // Swap front and back rendering buffers
        glfwSwapBuffers ();
        // Check if ESC key was pressed or window was closed
        running = !glfwGetKey (GLFW_KEY_ESC) && glfwGetWindowParam (GLFW_OPENED);
    }

    // Close window and terminate GLFW
    glfwTerminate ();
    
    //delete p;
    // Exit program
    return 0;
}

