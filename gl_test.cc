
#include <GL/glfw.h>
#include <string>
#include <list>
#include <iostream>
#include <boost/foreach.hpp>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

using namespace std;

typedef pair<int64_t, int32_t*> CountVertexPair;

class Patch {
    public:
        Patch(string filename) 
        {
            FILE* f = fopen(filename.c_str(), "rb");
            int64_t nVertices = 0;
            while (fread(&nVertices, sizeof(nVertices), 1, f))
            {
                int32_t *data = new int32_t[nVertices*2];
                if (1 != fread(data, nVertices*8, 1, f))
                    assert(false && "error reading file");
                
                polygons.push_back( CountVertexPair(nVertices, data));
            }
        }
        
        ~Patch()
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

int main () {
    int running = 1;

    //Patch *p = new Patch("../OSM/output/coast/seg#");
    
    Patch p("../OSM/output/coast/seg#");
    Patch p0("../OSM/output/coast/seg#0");
    Patch p2("../OSM/output/coast/seg#2");
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

        glColor3f(0,0,0);
        p.render();
        glColor3f(1,1,1);
        p0.render();
        p2.render();
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

