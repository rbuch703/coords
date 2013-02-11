
#include <GL/glfw.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <errno.h>

#include <sys/stat.h>

#include <malloc.h>
#include <string.h>


typedef struct vertex_data_t
{
    int64_t num_vertices;
    int32_t *vertices;
} vertex_data_t;

int isClockwise(vertex_data_t polygon)
{
    int32_t *vertex_data = polygon.vertices;
    assert( polygon.num_vertices >= 4);
    assert( vertex_data[0] == vertex_data[(polygon.num_vertices-1)*2]);
    assert( vertex_data[1] == vertex_data[(polygon.num_vertices-1)*2+1]);

    //int32_t *min_vertex = {vertex_data[0], vertex_data[1]};
    uint32_t min_idx = 0;
    for (int i = 0; i < polygon.num_vertices; i++)
        if ( vertex_data[2*i] < vertex_data[min_idx] ||
           ((vertex_data[2*i]== vertex_data[min_idx])&&
            (vertex_data[2*i+1] < vertex_data[min_idx+1]) ) )
            min_idx = i;
            
    uint32_t pred_idx = min_idx == 0 ? polygon.num_vertices-1 : min_idx-1;
    uint32_t succ_idx = min_idx == polygon.num_vertices-1 ? 0: min_idx+1;

    int64_t end_x = vertex_data[2*succ_idx];
    int64_t end_y = vertex_data[2*succ_idx+1];
    
    int64_t start_x = vertex_data[2*pred_idx];
    int64_t start_y = vertex_data[2*pred_idx+1];
    
    int64_t x = vertex_data[2*min_idx];
    int64_t y = vertex_data[2*min_idx];
    
    int64_t pseudodistance = (end_x - start_x)*(start_y - y) - (end_y - start_y)*(start_x - x);
    #warning undefined behavior in case of collinear vertices
    //assert ( pseudodistance != 0 && "colinear vertices");
    
    return pseudodistance < 0;
}

typedef struct tile_t
{
    vertex_data_t *polygons;
    int64_t num_polygons;
    int64_t num_max_polygons;
} tile_t;

tile_t read_tile(const char* filename)
{
    tile_t tile = { malloc( 64 * sizeof(vertex_data_t)), 0, 64};
    
    printf("opening file %s\n",filename);
    
    FILE* f = fopen(filename, "rb");
    assert (f != NULL);
    
    int64_t nVertices = 0;
    while (fread(&nVertices, sizeof(nVertices), 1, f))
    {
    
        vertex_data_t polygon = {nVertices, malloc( sizeof(int32_t) * 2 * nVertices) };
    
        if (1 != fread(polygon.vertices, nVertices*8, 1, f))
            assert( 0 && "error reading file");
        
        if (tile.num_polygons == tile.num_max_polygons)
        {
            tile.polygons = realloc(tile.polygons, 2 * tile.num_max_polygons * sizeof(vertex_data_t));
            tile.num_max_polygons *= 2;
        }

        tile.polygons[tile.num_polygons++] = polygon;        
    }
    fclose(f);
    return tile;
}

void free_tile(tile_t tile)
{
    while (tile.num_polygons)
    {
        free(tile.polygons[-- (tile.num_polygons)].vertices );
    }
    free(tile.polygons);
}

void render_tile(tile_t tile)
{

    //glColor3f(1,1,1);
    //BOOST_FOREACH( CountVertexPair p, polygons)
    for (int i = 0; i < tile.num_polygons; i++) 
    {
        glBegin(GL_LINE_STRIP);
            if (isClockwise(tile.polygons[i]))
                glColor3f(1,1,1);
            else
                glColor3f(0,0,0);
            
            int32_t* v = tile.polygons[i].vertices;
            for (int64_t num_vertices = tile.polygons[i].num_vertices; num_vertices; num_vertices--, v+=2)
            {
                glVertex2d(v[1], v[0]);
            }
        glEnd();
    }
}

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

void render_path(const char* filename)
{

    tile_t tile = read_tile(filename);
    render_tile(tile);
    free_tile(tile);
    
}

typedef struct rect_t
{
    double top, left, bottom, right;
} rect_t;

double max(double a, double b) { return a> b ? a : b;}
double min(double a, double b) { return a< b ? a : b;}
double width(rect_t rect) { return rect.right - rect.left;}
#warning using backup tiles
static const char* BASEPATH = "output/coast.bak/seg#";

void render(const rect_t view, rect_t tile, char *position)
{
    struct stat dummy;
    
    int buf_size = strlen(BASEPATH) + strlen(position) + 2;
    char *path = calloc( 1, buf_size ); // plus zero termination and an additional character (see below)
    // path = BASEPATH + position
    snprintf(path, buf_size, "%s%s", BASEPATH, position);
    
    
    if ( stat( path, &dummy ) != 0) 
    {   
        free(path);
        return;
    }

    assert (
        (max( view.left, tile.left) <= min( view.right, tile.right)) && 
        (max( view.bottom, tile.bottom) <= min( view.top, tile.top) ));
    
    /* if no child exists, then this tile is the highest resolution tile available 
     * at least one child exists, then the the existing tiles provide higher resolution for the respective areas.
     * Since in some areas, no data may to be drawn at all, some children may not exist */
    int hasChildren = 0;
    
    static const char *chars = "0123";
    for (int i = 0; i < 4; i++)
    {
        path[buf_size-2] = chars[i]; //overwrites the zero termination; but since another 0x00 follows, the string is still valid
        hasChildren |= stat( path, &dummy ) == 0;
    }
        
    int sufficientResolution = width(tile) < width(view);
    if (!hasChildren || sufficientResolution) 
    {
        path[buf_size-2] = '\0'; 
        //cout << "rendering tile " << position << endl;
        render_path(path);
        free(path);
        //Tile(BASEPATH+position).render(); 
        return;
    }
    free(path);
    
    double mid_x = (tile.right+tile.left) / 2.0;
    double mid_y = (tile.top + tile.bottom) / 2.0;
    
    rect_t tl2 = {tile.top, tile.left, mid_y,       mid_x};
    rect_t bl0 = {mid_y,    tile.left, tile.bottom, mid_x};
    rect_t tr3 = {tile.top, mid_x,     mid_y,       tile.right};
    rect_t br1 = {mid_y,    mid_x,     tile.bottom, tile.right};
    
    int len = strlen(position);
    char* pos = calloc(1,  len + 2);
    strncpy(pos, position, len);
    
    if ( mid_x > view.left) //has to render left half
    {
        pos[len] = '0';
        if (mid_y > view.bottom) render(view, bl0, pos);
        pos[len] = '2';
        if (mid_y < view.top   ) render(view, tl2, pos);
    }
    
    if ( mid_x < view.right) //has to render right half
    {
        pos[len] = '1';
        if (mid_y > view.bottom) render(view, br1, pos);
        pos[len] = '3';
        if (mid_y < view.top   ) render(view, tr3, pos);
    }

    free(pos);
}

int main () {
    int running = 1;

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
    // Main loop
    while (running) {
        
        // OpenGL rendering goes here...
        glClearColor(0.58f, 0.66f, 0.78f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(g_left, g_right, g_bottom, g_top, 0, 10);

        glColor3f(1,1,1);
        
        rect_t view ={ g_top, g_left, g_bottom, g_right };
        rect_t world={ 900000000, -1800000000, -900000000, 1800000000};
        render( view, world, "");
        // Swap front and back rendering buffers
        glfwSwapBuffers ();
        // Check if ESC key was pressed or window was closed
        running = !glfwGetKey (GLFW_KEY_ESC) && glfwGetWindowParam (GLFW_OPENED);
    }

    // Close window and terminate GLFW
    glfwTerminate ();
    
    return 0;
}

