
#include "quadtree.h"

/* approach: inductive insertion; we start with an empty tree, which naturally contains no intersections.
 *           The line segments are then added one by one in a way that guarantees that all intersections
 *           caused by the insertion are resolved, and thus the quad tree stays intersection-free
 *
 * Steps for each recursion depth:
 *      1. Test segment against all segments of the current quad tree node
 *          - if this creates new line segments, add them to the list of segments to be inserted ('createdSegments')
 *          - if this modifies segments from the current node, move them back to the list of segments to be inserted
 *          - if this modified the segment at hand, move it back to the list of segments to be inserted and RETURN;
 *      2. Check whether the segment fits completely into one of the four sub quadrants
 *          - if it does, call this method recursively for the sub quadrant; afterwards RETURN
 *          - if it does not, recursively test it against all sub quadrants through which it passes 
 *            ( if it intersects anything, proceed as in step 1)
 *      3. If it did not intersect anything in the previous step, add it to the list of segments for the current quad tree node and RETURN
 */
 
void moveIntersectionsToIntegerCoordinates3(list<LineSegment> &segments)
{
    assert( segments.begin() != segments.end() ); //has at least one segment; otherwise the next line would fail
    AABoundingBox box( segments.begin()->start);
    
    for (list<LineSegment>::iterator edge = segments.begin(); edge != segments.end(); edge++)
    {
        box += edge->start;
        box += edge->end;
    }

    QuadTreeNode root(box);// = new QuadTreeNode(box);
    
    while (segments.begin() != segments.end())
    {
        LineSegment seg = segments.front();
        segments.pop_front();
        root.insertIntoHierarchy(seg, segments);
    }
    
    root.exportSegments(segments);
    //root.printHierarchy();
}

void QuadTreeNode::exportSegments(list<LineSegment> &segments_out)
{
    segments_out.splice(segments_out.begin(), segments);
    if (tl) tl->exportSegments(segments_out);
    if (tr) tr->exportSegments(segments_out);
    if (bl) bl->exportSegments(segments_out);
    if (br) br->exportSegments(segments_out);
}

void QuadTreeNode::printHierarchy(int depth)
{
    for (int i = 0; i < depth; i++) cout << "  ";
    
    cout << "size: " << segments.size() << endl;
    if (tl) tl->printHierarchy(depth+1);
    if (tr) tr->printHierarchy(depth+1);
    if (bl) bl->printHierarchy(depth+1);
    if (br) br->printHierarchy(depth+1);
}



QuadTreeNode::QuadTreeNode(AABoundingBox _box): tl(NULL), tr(NULL), bl(NULL), br(NULL), box(_box)
{   
    mid_x = (box.left + box.right) / 2;
    mid_y = (box.top + box.bottom) / 2;
}

QuadTreeNode::~QuadTreeNode()
{
    delete tl;
    delete tr;
    delete bl;
    delete br;
}

bool QuadTreeNode::intersectedByRecursive( LineSegment edge1, list<LineSegment> &createdSegments)
{
   for (list<LineSegment>::iterator edge2 = segments.begin(); edge2 != segments.end(); edge2++)
   {
        LineSegment edge1_aux, edge2_aux;
        bool edge1_modified, edge2_modified;
        handleIntersection (edge1, *edge2, edge1_aux, edge2_aux, edge1_modified, edge2_modified);

        if (edge1_aux) createdSegments.push_back(edge1_aux);    //newly created segments need to be checked against the whole hierarchy as well
        if (edge2_aux) createdSegments.push_back(edge2_aux);
        
        if (edge2_modified)
        {
            if ((bool) *edge2) createdSegments.push_back(*edge2);
            edge2 = segments.erase(edge2);
        }
        
        if (edge1_modified)
        {
            if ((bool) edge1) createdSegments.push_back(edge1);
            return true;
        }
    }
    
    assert((tl && tr && bl && br) || (!tl && !tr && !bl && !br));
    if (!tl) return false; //no further child nodes to check --> no intersection took place
    
    bool in_tl, in_tr, in_bl, in_br;
    coversQuadrants( edge1, in_tl, in_tr, in_bl, in_br);
    
    if (in_tl && tl->intersectedByRecursive( edge1, createdSegments)) return true;
    if (in_tr && tr->intersectedByRecursive( edge1, createdSegments)) return true;
    if (in_bl && bl->intersectedByRecursive( edge1, createdSegments)) return true;
    if (in_br && br->intersectedByRecursive( edge1, createdSegments)) return true;
    return false;
}

bool QuadTreeNode::addToQuadrant(LineSegment seg)
{
    assert (tl && tr && bl && br);
    
    if      (seg.start.x < mid_x && seg.end.x < mid_x && seg.start.y < mid_y && seg.end.y < mid_y) { tl->segments.push_front(seg); return true; }
    else if (seg.start.x < mid_x && seg.end.x < mid_x && seg.start.y > mid_y && seg.end.y > mid_y) { bl->segments.push_front(seg); return true; }
    else if (seg.start.x > mid_x && seg.end.x > mid_x && seg.start.y < mid_y && seg.end.y < mid_y) { tr->segments.push_front(seg); return true; }
    else if (seg.start.x > mid_x && seg.end.x > mid_x && seg.start.y > mid_y && seg.end.y > mid_y) { br->segments.push_front(seg); return true; }
    
    return false;   //does not fit into any quadrant
}

void QuadTreeNode::subdivide()
{
    assert (!tl && !tr && !bl && !br);
    tl = new QuadTreeNode( AABoundingBox(box.top, box.left, mid_y, mid_x    ));
    tr = new QuadTreeNode( AABoundingBox(box.top, mid_x,    mid_y, box.right));
            
    bl = new QuadTreeNode( AABoundingBox(mid_y, box.left, box.bottom, mid_x    ));
    br = new QuadTreeNode( AABoundingBox(mid_y, mid_x,    box.bottom, box.right));
    
    for (list<LineSegment>::iterator edge = segments.begin(); edge != segments.end(); )
    {
        if (addToQuadrant(*edge))
            edge = segments.erase(edge);
        else
            edge++;
    }
}

/*bool intersect(LineSegment box_edge, LineSegment edge)
{

    BigInt num1 = (A.end.x - A.start.x) * (B.start.y-A.start.y) - (A.end.y-A.start.y)*(B.start.x-A.start.x);
    BigInt denom1=(A.end.y - A.start.y) * (B.end.x  -B.start.x) - (A.end.x-A.start.x)*(B.end.y  -B.start.y);

    BigInt num2 = (B.end.x - B.start.x) * (A.start.y-B.start.y) - (B.end.y-B.start.y)*(A.start.x-B.start.x);
    BigInt num2a= (B.end.x - B.start.x) * (B.start.y-A.start.y) - (B.end.y-B.start.y)*(B.start.x-A.start.x);
    BigInt denom2=(B.end.y - B.start.y) * (A.end.x  -A.start.x) - (B.end.x-B.start.x)*(A.end.y  -A.start.y);

    assert (denom1 == -denom2); //if true, one denominator computation can be removed
    assert (num2 == - num2a);   //if true, we could use num2a, which shares some terms with num1
}*/

/*struct FVertex
{
    FVertex(BigFration _x, BigFraction _y): x(_x), y(_y) { }
    BigFraction x,y;
}*/

//#error rewrite this, code is incorrect (is bases on intersections of lines, which may not coincide with intersections of the corresponding line segments)
int QuadTreeNode::coversQuadrants( LineSegment edge, bool &tl, bool &tr, bool &bl, bool &br) const
{
    static int covers = 0;
    covers++;
    BigInt mid_x = (box.right + box.left) / 2;
    BigInt mid_y = (box.top + box.bottom) / 2;
    
    tl = tr = bl = br = false;

    /*normalization, is necessary for orientation tests later on
     *better do it now so that 'edge' does not have to change orientation during the computation*/
    if (edge.start > edge.end)  
    {
        Vertex tmp = edge.start;
        edge.start = edge.end;
        edge.end   = tmp;
    }

    /*LineSegment horizontal( Vertex(box.left, mid_y), Vertex(box.right, mod_y));
    LineSegment vertical  ( Vertex(mid_x, box.top), vertex(mid_x, box.bottom));*/
    
    /** handle the special cases of a horizontal or vertical line
      * Only for these cases can the decisions whether the line is in the top/bottom half
      * and in the left/right half be made separately and then correctly be merged
      * (e.g. if a horzontal/vertical line passes the top half and the left half,
      * it has to pass the top-left quadrant. A general line that passes the top and left half
      * may pass the top-right and bottom-left (and additionally the bottom-right) quadrants
      * and thus never pass top-left quadrant, even though it passes the top and left halves.
      *
      * for each of the two special cases, either min_x == max_x or min_y == max_y holds.
      * So the code could be split into two separate cases for horizontal/vertical lines,
      * an in each case a min and max computatin could be omitted. They are merged here simply for brevity
      */
    if ((edge.start.x == edge.end.x)|| (edge.start.y == edge.end.y)) 
    {
        BigInt min_x = edge.start.x < edge.end.x ? edge.start.x : edge.end.x;
        BigInt max_x = edge.start.x > edge.end.x ? edge.start.x : edge.end.x;

        bool left =  (min_x <= mid_x) && (max_x >= box.left); 
        bool right = (max_x >= mid_x) && (min_x <= box.right);
        
        BigInt min_y = edge.start.y < edge.end.y ? edge.start.y : edge.end.y;
        BigInt max_y = edge.start.y > edge.end.y ? edge.start.y : edge.end.y;
        
        bool top =   (min_y <= mid_y) && (max_y >= box.top);
        bool bottom= (max_y >= mid_y) && (min_y <= box.bottom);
        
        tl = top & left;
        tr = top & right;
        bl = bottom & left;
        br = bottom & right;
        return (tl ? 1 : 0) + (tr ? 1 : 0) + (bl ? 1 : 0) + (br ? 1 : 0);
    }

    /* approach for general edge positions:
     * 1a. if line endpoint lies inside 'box', flag the quadrants in which the endpoints lie as 'true'
     * 1b. if an endpoint lies outside of 'box', flag the quadrant through which the line leaves 'box' as 'true'
     * 2.  count number of quadrants flagged as 'true'
     *      - 0 line does not pass 'box', RETURN
     *      - 1 line enters and leaves through the same quadrant, can't cross any other quadrant in between, RETURN
     *      - 2 several cases are possible, see below
     *      - 3 at least one endpoint lies *on* the line between two quadrants; since a non-AA edge 
                cannot pass more than three quadrants, all quadrants have been found; RETURN
     *      - 4 BUG: endpoints cannot lie in all four quadrants unless 'edge' is axis parallel (which 
                would already have been handled above), assert(false) and RETURN
     * 3. If the function has not RETURNed, the endpoints are located in exactly two quadrants
     *      - if the quadrants are adjacent, 'edge' cannot cross any other quadrant, RETURN
     *      - if the quadrants are non-adjacent (i.e. are opposite corners), at least one additional quadrant is passed
     *          - if the pseudodistance of (mid_x, mid_y) to 'edge' is zero, 'edge' passes through the point at which 
     *            all four quadrants meet --> 'edge' passes through all four quadrants, RETURN
     *          - otherwise use the sign of the pseudodistance to determine, which one of the other two quadrants
     *            is also passed, RETURN
     */
     
     // start vertex is inside 'box'
     if (edge.start.x >= box.left && edge.start.x <= box.right && edge.start.y >= box.top && edge.start.y <= box.bottom)
     {
        bool left  = edge.start.x <= mid_x;
        bool right = edge.start.x >= mid_x; //through the equality ("="), 'left' and 'right' as well as 'top' and 
        bool top   = edge.start.y <= mid_y; //'bottom' can be true at the same time. This is intentional.
        bool bottom= edge.start.y >= mid_y;
        assert (left | right);
        assert (top | bottom);
        tl = top & left;
        tr = top & right;
        bl = bottom & left;
        br = bottom & right;
        if (tl && tr && bl && br) return 4;
     } else //start vertex is outside the box, need to check were 'edge' left the box (if it passes it at all)
     {
        assert (edge.start.x < edge.end.x);
        BigFraction x, y;
        if (edge.intersects( LineSegment(box.left, box.top, box.left, box.bottom),x,y)) //test against left box edge
        {
            assert(x == box.left);
            tl = (y <= mid_y);
            bl = (y >= mid_y);
        } else //'edge' starts left of the box and does not intersect its left edge 
               //--> intersects either top or bottom edge, or does not pass the box at all
        {   
            assert (edge.start.y != edge.end.y);
            if (edge.start.y < edge.end.y) // 'edge' goes from top-left to bottom-right --> can only intersect *top* edge on the left
            {
                BigFraction x,y;
                if (!edge.intersects( LineSegment(box.left, box.top, box.right, box.top), x, y)) 
                    return 0; //does not pass 'box'
                assert (y == box.top);
                tl = (x <= mid_x);
                tr = (x >= mid_x);
            }
            else    //'edge' goes from bottom-left to top-right --> can only intersect bottom edge on the left
            {
                BigFraction x,y;
                if (!edge.intersects( LineSegment(box.left, box.bottom, box.right, box.bottom),x,y)) 
                    return 0; //does not pass box
                assert (y == box.bottom);
                bl = (x <= mid_x);
                br = (x >= mid_x);
            }
        }
     }
     assert (tl | tr | bl | br);

     // end vertex is inside 'box'
     if (edge.end.x >= box.left && edge.end.x <= box.right && edge.end.y >= box.top && edge.end.y <= box.bottom)
     {
        bool left  = edge.end.x <= mid_x;
        bool right = edge.end.x >= mid_x; //through the equality ("="), 'left' and 'right' as well as 'top' and 
        bool top   = edge.end.y <= mid_y; //'bottom' can be true at the same time. This is intentional.
        bool bottom= edge.end.y >= mid_y;
        assert (left | right);
        assert (top | bottom);
        tl |= top & left;
        tr |= top & right;
        bl |= bottom & left;
        br |= bottom & right;
        if (tl && tr && bl && br) return 4;
     } else
     { //end vertex is outside the box, need to check were 'edge' left the box (if it passes it at all)
        assert (edge.start.x < edge.end.x);        
        BigFraction x, y;
        if (edge.intersects( LineSegment(box.right, box.top, box.right, box.bottom),x,y)) //test against right box edge
        {
            assert(x == box.right);
            tr |= (y <= mid_y);
            br |= (y >= mid_y);
        } else //'edge' ends right of the box, but does not intersect its right edge.
               //--> intersects either top or bottom edge, or does not pass the box at all
        {   
            assert (edge.start.y != edge.end.y);
            if (edge.end.y < edge.start.y ) // 'edge' goes from bottom-left to top-right --> can only intersect *top* edge on the right
            {
                BigFraction x,y;
                if (!edge.intersects( LineSegment(box.left, box.top, box.right, box.top), x, y))
                    return 0; //does not pass 'box'
                assert (y == box.top);
                tl |= (x <= mid_x);
                tr |= (x >= mid_x);
            }
            else    //'edge' goes from top-left to bottom-right--> can only intersect bottom edge on the right
            {
                BigFraction x,y;
                if (!edge.intersects( LineSegment(box.left, box.bottom, box.right, box.bottom),x,y) )
                    return 0; //does not pass box
                assert (y == box.bottom);
                bl |= (x <= mid_x);
                br |= (x >= mid_x);
            }
        }
     }
    
    int num_quadrants = (tl ? 1 : 0) + (tr ? 1 : 0) + (bl ? 1 : 0) + (br ? 1 : 0);

    // these cases cannot happen: 0 would have been handled before, and
    // 4 quadrants for entry/exit points is impossible for non-AA edges 
    // (unless one edge vertex is at (mid_x, mid_y), which has already been handled above
    assert (num_quadrants != 0 && num_quadrants != 4);
    
    if (num_quadrants ==1 ) return 1; // all end points /exist points in same quadrant --> no more quadrants crossable
    if (num_quadrants ==3 ) return 3; // see 'general approach' above
    
    assert (num_quadrants == 2);
    
    if ( (tl || br) && (tr || bl)) return 2; //two *adjacent* quadrants, edge cannot cross any more quadrants


    BigInt dist = Vertex(mid_x, mid_y).pseudoDistanceToLine(edge.start, edge.end);
    if (dist == 0) { tl = tr = bl = br = true; return 4;} // edge passes through center (where all quadrants meet) --> passes all quadrants

    if ( tl && br) //tl and br quadrant --> edge has to pass through at lest one other quadrant to connect these two
    {
        bl = dist <  0; //edge from top-left to bottom-right; (mid_x,mid_y) lies left of edge --> edge passes through bl quadrant
        tr = dist >  0; //edge from tl to br; (mid_x, mid_y) lies right of edge --> edge passes through tr quadrant
    }
    
    if ( bl && tr)
    {
        br = (dist <  0); //edge from bottom-left to top-right; (mid_x,mid_y) lies left of edge --> edge passes through br quadrant
        tl = (dist >  0); //edge from bl to tr; (mid_x, mid_y) lies right of edge --> edge passes through tr quadrant
    }
    return 3;
}

/* 1. Determine whether the edge intersects with any edge from the current QuadTreeNode
 *    - if it does (and the edge is modified in the process), add it to 'createdSegments' and RETURN
 * 2. Test whether the edge would fit into any existing sub-quadrant
 * 2a.If it does, call insertIntoHierarchy recursively for that quadrant
 * 2b.If it does not, recursively ensure that the edge does not intersect with any edge
 *    *below* the current QuadTreeNode (parent QuadTreeNodes up to this node were already tested through the recursion)
 *      - if it does (and the edge is modified in the process), add it to 'createdSegments' and RETURN
 * 3. Add it to the current QuadTreeNode
 * 4. If the number of edges in the current QuadTreeNode now exceeds a given threshold, subdivide it
 */
void QuadTreeNode::insertIntoHierarchy( LineSegment edge1, list<LineSegment> &createdSegments)
{
    for (list<LineSegment>::iterator edge2 = segments.begin(); edge2 != segments.end(); edge2++)
    {
        LineSegment edge1_aux, edge2_aux;
        bool edge1_modified, edge2_modified;
        handleIntersection (edge1, *edge2, edge1_aux, edge2_aux, edge1_modified, edge2_modified);

        if (edge1_aux) createdSegments.push_back(edge1_aux);    //newly created segments need to be checked against the whole hierarchy as well
        if (edge2_aux) createdSegments.push_back(edge2_aux);
        
        if (edge2_modified)
        {
            if ((bool) *edge2) createdSegments.push_back(*edge2);
            edge2 = segments.erase(edge2);
        }
        
        if (edge1_modified)
        {
            if ((bool) edge1) createdSegments.push_back(edge1);
            return; //old edge1 no longer valid, may have been changed and thus must be checked against whole hierarchy again
        }
    }
    
    
    if (!tl) //this QuadTreeNode is a leaf --> no more intersection tests necessary
    {
        assert (!tr && !bl && !br);
        segments.push_back(edge1);
        if ( box.width() >= 2 && box.height() >= 2 && segments.size() > SUBDIVISION_THRESHOLD)
            subdivide();
        return;
    }
    
    // if this point is reached, 'edge' does not intersect any edge on this QuadTreeNode, and this node has children
    bool in_tl, in_tr, in_bl, in_br;
    int num_quads = coversQuadrants( edge1, in_tl, in_tr, in_bl, in_br);
    if (num_quads == 1)
    {
        if      (in_tl) return tl->insertIntoHierarchy(edge1, createdSegments);
        else if (in_tr) return tr->insertIntoHierarchy(edge1, createdSegments);
        else if (in_bl) return bl->insertIntoHierarchy(edge1, createdSegments);
        else if (in_br) return br->insertIntoHierarchy(edge1, createdSegments);
        else assert(false);
        return;
    }
    
    /* if this point is reached, 'edge' does not intersect any edge on this QuadTreeNode, and this node 
     * has children. But edge still needs to be but here, because it would pass through more than one child node
     * Before it can be added to 'segments', however, we need to verify that 'edge' does not intersect with any
     * edge from any child node */
     
     if ( tl->intersectedByRecursive( edge1, createdSegments)) return;
     if ( tr->intersectedByRecursive( edge1, createdSegments)) return;
     if ( bl->intersectedByRecursive( edge1, createdSegments)) return;
     if ( br->intersectedByRecursive( edge1, createdSegments)) return;
     
    segments.push_back(edge1);
    //no need to check if this node should be subdivided, this point is only reached if the node is already subdivided
    assert (tl && tr && bl && br);
} 
