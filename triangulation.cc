
#include "triangulation.h"

#include "vertexchain.h"
#include <boost/foreach.hpp>


#include "config.h"
#include "geometric_types.h"
#include "vertexchain.h"
#include "helpers.h"

#include <list>


SimpEvent::SimpEvent( Vertex p_pred, Vertex p_pos, Vertex p_aux, Vertex p_succ, bool p_isShared, EventType p_type): 
        pred( p_pred), pos(  p_pos), aux(  p_aux), succ( p_succ), isShared(p_isShared), type(p_type)
{
    assert ( isShared ? (pos.get_x() == aux.get_x()) : p_aux == Vertex(0,0)  );
    //assert( (vertices.front() != vertices.back()) && "For this algorithm, the duplicate back vertex must have been removed");
    //cout << "creating event " << pred << "/" << pos << "/" << succ << endl;
}
    
bool SimpEvent::operator==(const SimpEvent & other) const {return pos == other.pos; }
bool SimpEvent::operator!=(const SimpEvent & other) const {return pos != other.pos; }
bool SimpEvent::operator <(const SimpEvent &other) const  {return pos <  other.pos; }


ostream &operator<<( ostream &os, SimpEvent ev)
{
    switch (ev.type)
    {
        case START:  os << "START";  break;
        case END:    os << "END";    break;
        case SPLIT:  os << "SPLIT";  break;
        case MERGE:  os << "MERGE";  break;
        case REGULAR:os << "REGULAR";break;
        default: assert(false); break;
    }
    os << " *" << ev.pos << "* : " << ev.pred <<" [" << ev.aux << "] " << ev.succ;
    return os;
}



// ===========================================================

/** returns whether the line 'a' has a smaller or equal y-value at xPos than the line 'b'.
    The method silently assumes that both segments actually intersect the vertical line at xPos.
    Do not make this a method os the LineSegment class, as it has nothing to do with it semantically
  */
bool leq(const LineSegment a, const LineSegment b, int32_t xPos)
{
    assert(a.start.get_x() != a.end.get_x());
    assert(b.start.get_x() != b.end.get_x());
    
    BigInt dbx = b.end.get_x() - b.start.get_x();
    BigInt dax = a.end.get_x() - a.start.get_x();
    
    BigInt dby = b.end.get_y() - b.start.get_y();
    BigInt day = a.end.get_y() - a.start.get_y();
    
    bool flipSign = (dax < 0) != (dbx < 0);
    
    BigInt left = (a.start.get_y() - b.start.get_y()) * dbx * dax;
    BigInt right=  dby * dax * (xPos - b.start.get_x()) - day * dbx * (xPos - a.start.get_x());
    
    return (left == right)|| ((left < right) != flipSign);
}

bool eq(const LineSegment a, const LineSegment b, int32_t xPos)
{
    assert(a.start.get_x() != a.end.get_x());
    assert(b.start.get_x() != b.end.get_x());
    
    BigInt dbx = b.end.get_x() - b.start.get_x();
    BigInt dax = a.end.get_x() - a.start.get_x();
    
    BigInt dby = b.end.get_y() - b.start.get_y();
    BigInt day = a.end.get_y() - a.start.get_y();
    
    BigInt left = (a.start.get_y() - b.start.get_y()) * dbx * dax;
    BigInt right=  dby * dax * (xPos - b.start.get_x()) - day * dbx * (xPos - a.start.get_x());
    
    return left == right;
}

//=====================================

// returns whether the line segment 'a' at x-value xPos has a y-value less than or equal to yPos
bool leq(const LineSegment a, Vertex pos)
{
    int64_t dax = (int64_t)a.end.x - (int64_t)a.start.x;
    
    assert ((( a.start.x >= pos.x && a.end.x  <= pos.x) ||
             ( a.end.x   >= pos.x && a.start.x<= pos.x)) &&
             "position not inside x range of segment");
             
    assert (dax != 0 && "edge case vertical line segment");
    
    int64_t day = (int64_t) a.end.y - (int64_t)a.start.y;
    int64_t dx =  (int64_t)pos.x - (int64_t)a.start.x;
    int64_t dy =  (int64_t)pos.y - (int64_t)a.start.y;
    
    int64_t left = dx * day;
    int64_t right= dy * dax;
    
    bool flipSign = (dax < 0);
    
    return (left == right) || ((left < right) != flipSign);
}

bool eq(const LineSegment a, Vertex pos)
{
    int64_t dax = (int64_t)a.end.x - (int64_t)a.start.x;
    
    assert ((( a.start.x >= pos.x && a.end.x  <= pos.x) ||
             ( a.end.x   >= pos.x && a.start.x<= pos.x)) &&
             "position not inside x range of segment");
             
    assert (dax != 0 && "edge case vertical line segment");
    
    int64_t day = (int64_t) a.end.y - (int64_t)a.start.y;
    int64_t dx =  (int64_t)pos.x - (int64_t)a.start.x;
    int64_t dy =  (int64_t)pos.y - (int64_t)a.start.y;
    
    int64_t left = dx * day;
    int64_t right= dy * dax;
    
    return (left == right);
}

typedef AVLTreeNode<LineSegment> *EdgeContainer;

 // protected inheritance to hide detail of AVLTree, since for a LineArrangement, AVLTree::insert must never be used
class LineArrangement: protected AVLTree<LineSegment>
{
public:
    EdgeContainer addEdge(const LineSegment &a, const BigFraction xPosition);
/*
#ifndef NDEBUG
    bool isConsistent(EdgeContainer node, BigInt xPos) const
    {
        bool left = !node->m_pLeft || 
                    (leq( node->m_pLeft->m_Data, node->m_Data, xPos)
                    && isConsistent(node->m_pLeft, xPos));
                    
        bool right= !node->m_pRight ||
                    (leq( node->m_Data, node->m_pRight->m_Data, xPos) && 
                    isConsistent(node->m_pRight, xPos));
        return left && right;
    }
    
    bool isConsistent(BigInt xPos) const { return !m_pRoot || isConsistent(m_pRoot, xPos); }
#endif */

    int size() const { return AVLTree<LineSegment>::size(); }
    
    
    bool hasPredecessor( EdgeContainer node)
    {
        iterator it(node, *this);
        return it != begin();
    }

    LineSegment getPredecessor( EdgeContainer node)
    {
        iterator it(node, *this);
        assert (it != begin());
        return *(--it);
    }
    
    /*
    bool hasSuccessor( EdgeContainer node )
    {
        iterator it(node, *this);
        return ++it != end();
    }
    
    LineSegment getSuccessor( EdgeContainer node)
    {
        iterator it(node, *this);
        assert (++it != end());
        return *it;
    }*/
  
    LineSegment getEdgeLeftOf( Vertex pos )
    {
        EdgeContainer e = findAdjacentEdge( pos );
        assert( e );
        if ( leq(e->m_Data, pos) )
            return e->m_Data;
        
        assert( hasPredecessor(e) );
        LineSegment edge = getPredecessor( e );
        assert( leq(edge, pos));
        return edge;
    }
    
    void remove(LineSegment item, int32_t xPos)
    {
        //cout << "\tremoving edge " << item << " at x= " << xPos << endl;
    
        assert( item.start.get_x() <= item.end.get_x());
/*#ifndef NDEBUG
        #warning expensive debug checks (that change the time complexity)
        for (const_iterator it = this->begin(); it != end(); it++)
            assert( it->start.get_x() <= xPos && it->end.get_x() >= xPos && "line arrangement contains outdated edge");
#endif*/

        AVLTreeNode<LineSegment> *node = findPos(item, xPos);
        assert( node && "Node to be removed does not exist");
        assert( node->m_Data == item);
        AVLTree<LineSegment>::remove(node);
    }
    
    EdgeContainer findPos(LineSegment item, int32_t xPos)
    {
    	if (! m_pRoot) return NULL;

	    //BigFraction yVal = item.getYValueAt(xPos);

	    AVLTreeNode<LineSegment>* pPos = m_pRoot;
	    while (pPos)
	    {
	        //BigFraction yVal2 = pPos->m_Data.getYValueAt(xPos);
	        if ( eq(item, pPos->m_Data,xPos) ) return pPos;
	        pPos = leq(item, pPos->m_Data, xPos) ? pPos->m_pLeft : pPos->m_pRight;
	    }
	    return pPos;
    }
    
    
    /* returns an adjacent edge to (xPos, yPos), i.e. an edge that is closest to (xPos, yPos)
       either to the left or to the right.
       If any edge passes directly through (xPos, yPos), the method is guaranteed to return said edge.
       Otherwise, the method makes no guarantee on which of the two adjacent edges it returns. In
       particular, it need not return the closest of the two edges.
   */
    EdgeContainer findAdjacentEdge(const Vertex v)
    {
    	if (! m_pRoot) return NULL;
    	
        EdgeContainer parent=m_pRoot;
	    EdgeContainer pPos = m_pRoot;
	    while (pPos)
	    {
	        parent = pPos;
	        if ( eq(pPos->m_Data, v) ) return pPos;
	        pPos = leq(pPos->m_Data, v) ? pPos->m_pRight : pPos->m_pLeft;
	    }
	    return parent;
    }
/*
    EdgeContainer findAdjacentEdge(Vertex v)
    {
        return findAdjacentEdge( v.get_x(), v.get_y() );
    }*/
    
    EdgeContainer insert(const LineSegment &item, int32_t xPos)
    {
        //cout << "\tinserting edge " << item << " at x=" << xPos << endl;
        
        // Must not be equal because LineArrangement internally sorts these lines by x-coordinate
        // and a segment with identical x-values at its entpoints does not have a unique x coordinate.
        // Must not be bigger, because our canonical form for status edges is define as having a start
        // with a smaller x-coordinate than the end. This reduced the numbers of necessary checks elsewhere
        assert( item.start.get_x() < item.end.get_x());
    
/*#ifndef NDEBUG
        #warning expensive debug checks (that change the time complexity)
        for (LineArrangement::const_iterator it = begin(); it != end(); it++)
            assert( it->start.get_x() <= xPos && it->end.get_x() >= xPos && "line arrangement contains outdated edge");
#endif*/
    
        EdgeContainer parent = findParent(item, xPos);
        
	    AVLTreeNode<LineSegment>* pPos = new AVLTreeNode<LineSegment>(0,0, parent, item);
	
	    if (!parent)
	    {
	        m_pRoot = pPos;
	        pPos->m_dwDepth = 0;    //no children --> depth=0
	        num_items++;
	        return pPos;   //no parent --> this is the root, nothing more to do
	    }
	
	    assert (!eq(item, parent->m_Data, xPos));
	    
	    if ( leq(item, parent->m_Data, xPos) )
	    {
	        assert( ! parent->m_pLeft);
	        parent->m_pLeft = pPos;
	    } else
	    {
	        assert( ! parent->m_pRight);
	        parent->m_pRight = pPos;
	    }
	
	    updateDepth( pPos);//, NULL, NULL);
	    num_items++;
	    return pPos;
    }
    
protected:

    /* returns the tree node whose one direct child would be 'item' (if 'item' was in the tree).
     * This method assumes that 'item' is not actually present in the tree */
    EdgeContainer findParent(const LineSegment &item, int32_t xPos) const
    {
	    EdgeContainer parent = NULL;
	    if (! m_pRoot) return NULL;

	    EdgeContainer pPos = m_pRoot;
	    while ( ! eq( pPos->m_Data, item, xPos) )
	    {
		    parent = pPos;
		    pPos = leq(item, pPos->m_Data, xPos) ? pPos->m_pLeft : pPos->m_pRight;
		    if (! pPos) return parent;
	    }
	    assert(false && "duplicate entry found");
	    
	    return pPos->m_pParent;
    }
/*    
	AVLTree::iterator getIterator(LineSegment e, const BigInt xPos)
	{
	    AVLTreeNode<LineSegment>* p = findPos(e, xPos);
	    assert(p);
	    assert(p->m_Data == e);
	    return AVLTree::iterator(p, *this);
	}
*/
};


list<VertexChain> polygonsFromEndMonotoneGraph( map<Vertex, vector<Vertex>> graph, vector<Vertex> endVertices)
{
    list<VertexChain> res;
    for (vector<Vertex>::const_iterator it = endVertices.begin(); it != endVertices.end(); it++)
    {
        list<Vertex> chain;
        chain.push_back(*it);
        assert(graph[*it].size() == 2);
        
        Vertex v1 = graph[*it][0];
        Vertex v2 = graph[*it][1];
        
        int dist = v2.atSideOfLine(*it, v1);
        assert(dist != 0 && "colinear Vertices");
        
        Vertex top =  dist > 0 ? v2 : v1;
        Vertex bottom=dist > 0 ? v1 : v2;

        //cout << "starting at vertex " << *it << ", top: " << top << ", bottom: " << bottom << endl;
        
        while (top != bottom)
        {
            bool updateTop = top.get_x() >= bottom.get_x();
            //Vertex &update = () ? top : bottom;
            Vertex v = updateTop ? top : bottom;
            Vertex pred;
            if (updateTop)
            {
                pred = chain.front();
                chain.push_front(top);
            }
            else
            {
                pred = chain.back();
                chain.push_back(bottom);
            }
            //cout << "\tfinding " << ( updateTop ? "top" : "bottom") <<" successor vertex to " << v << endl;
            
            assert(graph[v].size() > 1); //at least 2 edges from the original polygon, potentially additional ones from diagonals
            Vertex vSucc;

            if (graph[v].size() == 2)
            {
                Vertex next1 = graph[v][0];
                Vertex next2 = graph[v][1];
                assert( (next1 == pred) != (next2 == pred)); //exactly one of the connected vertices has to be the predecessor
                vSucc = (next1 == pred) ? next2 : next1;
            } else
                vSucc = updateTop ? getLeftMostContinuation(graph[v], pred, v):
                                   getRightMostContinuation(graph[v], pred, v);

            //assert(hasSucc);
            //cout << "\t\tfound " << ( updateTop ? "top" : "bottom")<< vSucc << endl;
            assert(*it != v);
            if (updateTop)
                top = vSucc;
            else
                bottom= vSucc;                
            
        }
        chain.push_back(top);   //add the final vertex, top == bottom
        chain.push_front(top);  //... and duplicate it to mark it as a closed polygon
        
        //cout << "\tclosed polygon with vertex " << top << endl;
        VertexChain c(chain);
        c.canonicalize();
        //std::cout << c.isClockwise() << endl;
        //std::cout << c.data() << endl;
        assert(c.isClockwise());
        res.push_back(c);        
    }
    return res;
}


list<LineSegment> createEndMonotoneDiagonals( VertexChain &chain)
{
    assert( chain.data().front() == chain.data().back());
    assert( chain.isClockwise());
    /*cout << "=======================" << endl;
    for (uint64_t i = 0; i < chain.data().size(); i++)
        cout << "* " << chain.data()[i] << endl;*/
        
    vector<Vertex> verts(chain.data());
    verts.pop_back();
    
    assert( verts.size() >= 3);
    MonotonizeEventQueue events(verts);

    /*for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);*/

    map<LineSegment, Vertex> helpers;
    LineArrangement status;
    list<LineSegment> newDiagonals;
    while (events.size() != 0)
    {
        SimpEvent ev = events.pop();
        //cout << "handling event " << ev << endl;
        switch (ev.type)
        {
            case START:
            {
                status.insert( LineSegment(ev.pos, ev.pred), ev.pos.x );
                helpers.insert( pair<LineSegment, Vertex>( LineSegment(ev.pos, ev.pred), ev.pos));
                break;
            }
            case END: 
            {
                LineSegment e = status.getEdgeLeftOf(ev.pos);
                assert( e.end == (ev.isShared ? ev.aux : ev.pos));
                 //e.start == ev.pos  || e.end == ev.pos);

                helpers.erase( e);
                status.remove( e, ev.pos.x);
                
                break;
            }
            case SPLIT:
            {
                LineSegment edge = status.getEdgeLeftOf( ev.pos);
                assert( helpers.count( edge ));
                assert( helpers[edge].get_x() <= ev.pos.get_x());

                assert( !ev.isShared || (ev.pos.y > ev.aux.y) );
                
                /* The diagonal needs to end at the left-most (lower y-value) of the two (pos, aux) vertices:
                   For the general case (helper has lower x-value), any of the two would do. But for the edge case
                   that the helper has identical x-value and 'pos' and 'aux', the new diagonal for the helper must end
                   at the first of {pos, aux} that it encounters; otherwise the resulting graph would be inconsistent.
                   Since the helper has to be lexicographically smaller than the event (otherwise its event would occur 
                   after this one and thus he would not yet be a helper), in has to have smaller y-value than both 
                   'pos' and 'aux' if all share a common x-value. Since of 'pos' and 'aux', 'aux' always has the smaller
                   y-value for SPLIT events (checked via assertion above), 'aux' is the first vertex encountered 
                   when starting from the helper. */
                Vertex vLeft = ev.isShared ? ev.aux : ev.pos;
                newDiagonals.push_back( LineSegment( helpers[edge], vLeft));

                helpers[edge] = vLeft;
                
                status.insert( LineSegment(ev.pos, ev.pred), ev.pos.x );
                /* Following the same argument as above, the helper needs to be the right-most of {pos,aux}: 
                   In the edge case that pos, aux and a lexicographically *bigger* other SPLIT event share an 
                   x-value, the new diagonal will be between a position right of {pos, aux} and thus has to end
                   at 'pos' (and not pass through it to end at 'aux') */
                helpers.insert( pair<LineSegment, Vertex>( LineSegment(ev.pos, ev.pred), ev.pos));
                break;
            }
            case MERGE:
            {
                /* A MERGE event merges two ranges to yield one. It occurs at the vertex connecting the two ranges.
                   algorithm: 1. remove the right sub-range from 'status' and 'helpers' (since we now only have a single merged range)
                                 This will be a 'status' edge that (in non-degenerate cases) ends at ev.pos
                              2. set ev.pos as the new helper for the 'status' edge of the left sub-range (now the complete range)
                                 as ev.pos is not the knwon vertex with the highest x-value inside the range */
                assert( !ev.isShared || ( ev.pos.y < ev.aux.y ));
                
                Vertex vRightMost = ev.isShared ? ev.aux : ev.pos;
                LineSegment rightEdge = status.getEdgeLeftOf(vRightMost);
                //cout << "right edge is " << rightEdge << endl;
                assert( rightEdge.end == vRightMost );
                
                status.remove( rightEdge, ev.pos.x );
                assert( helpers.count(rightEdge) );
                helpers.erase( rightEdge );


                Vertex vLeftMost = ev.isShared ? ev.aux : ev.pos;
                LineSegment edge = status.getEdgeLeftOf( vLeftMost );
                assert( helpers.count(edge));
                /* helper needs to be the right-most of {pos, aux} for degenerate cases where the next SPLIT event also 
                 * has the same x-value (and higher y-value). */
                helpers[edge] = vRightMost; 
                break;
            }
            case REGULAR:
                
                //assert(  (pred.x != pos.x || succ.x != pos.x) && "Colinear vertices"); 

                bool isPolyRightOfEvent = (ev.pred.x >= ev.pos.x && ev.pos.x >= ev.succ.x);
                
                if (isPolyRightOfEvent)
                {
                    //cout << "\tPolygon is right of vertex." << endl;
                    LineSegment oldEdge = status.getEdgeLeftOf( ev.isShared ? ev.aux : ev.pos );
                    assert (oldEdge.end == (ev.isShared ? ev.aux :ev.pos) );
                    assert (helpers.count(oldEdge));
                    helpers.erase(oldEdge);
                    status.remove(oldEdge, ev.pos.x);
                    
                    LineSegment newEdge(ev.pos, ev.pred);
                    status.insert( newEdge, ev.pos.x);
                    Vertex rightMost = !ev.isShared || (ev.pos.y > ev.aux.y) ? ev.pos : ev.aux;
                    helpers.insert( pair<LineSegment, Vertex>( newEdge, rightMost));
                } else
                {
                    //cout << "\tPolygon is left of vertex." << endl;
                    assert( ev.succ.x >= ev.pos.x && ev.pos.x >= ev.pred.x );
                    Vertex leftMost = !ev.isShared || (ev.pos.y < ev.aux.y) ? ev.pos : ev.aux;
                    LineSegment leftEdge = status.getEdgeLeftOf(ev.pos);
                    assert(helpers.count(leftEdge));
                    helpers[leftEdge] = leftMost;
                }

                break;
        }
    }
        
    assert(status.size() == 0);
    assert(helpers.size()== 0);
    return newDiagonals;
}

map<Vertex, vector<Vertex> > toGraph(const VertexChain &chain, const list<LineSegment> newDiagonals)
{
    map<Vertex, vector<Vertex> >  graph;
    assert( chain.data().front() == chain.data().back());
    
    for (uint64_t i = 0; i < chain.size()-1; i++)   //don't need to traverse the last vertex, it's identical to the first one
    {
        Vertex v1 = chain.data()[i];
        Vertex v2 = chain.data()[i+1];
        
        if ( graph.count(v1) == 0) graph.insert( pair<Vertex, vector<Vertex> >(v1, vector<Vertex>() ));
        if ( graph.count(v2) == 0) graph.insert( pair<Vertex, vector<Vertex> >(v2, vector<Vertex>() ));
        
        graph[v1].push_back(v2);
        graph[v2].push_back(v1);
    }
    
    for (list<LineSegment>::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    {
        assert(graph.count(it->start)); //diagonals should only connect to vertices already presend in the 
        assert(graph.count(it->end));   //original polygon
        
        graph[it->start].push_back(it->end);
        graph[it->end].push_back(it->start);
    }

//
    //assert( graph.count(v));
    //for ( vector<Vertex>::const_iterator it = graph[v].begin(); it != graph[v].end(); it++)
    //    cout << "*" << *it << endl;
    //assert(false);
    return graph;
}

vector<Vertex> getEndVertices(const VertexChain &chain)
{
    vector<Vertex> poly( chain.data() );
    assert( poly.front() == poly.back());
    poly.pop_back();    //required for the modulus arithmetic in the event classification to work correctly
    
    vector<Vertex> res;
    MonotonizeEventQueue events(poly);
    while (events.containsEvents())
    {
        SimpEvent ev = events.pop();
        if (ev.type == END)
            res.push_back(ev.pos);
    }
    
    return res;
}

list<VertexChain> toMonotonePolygons( VertexChain &chain)
{
    if (chain.size() == 4) //triangle
    {
        list<VertexChain> res;
        res.push_back( chain );
        return res;
    }
    

/*    cout << "========== new edges ==========" << endl;
    BOOST_FOREACH( LineSegment seg, newDiagonals)
        cout << seg << endl;*/
        

    //cout << "==========================================" << endl;
    //list<LineSegment> dummy;
    //createCairoDebugOutput( chain, dummy);

    list<LineSegment> newDiagonals = createEndMonotoneDiagonals(chain);
    //createCairoDebugOutput( chain, newDiagonals);

    list<VertexChain> polygons = polygonsFromEndMonotoneGraph(toGraph(chain, newDiagonals), getEndVertices(chain));

    //createCairoDebugOutput( chain, newDiagonals);
    //cairo_render_polygons(polygons);
    
    list<VertexChain> res;
    for ( list<VertexChain>::iterator poly = polygons.begin(); poly != polygons.end(); poly++)
    {
        /*
        static int i = 0;
        i++;
        cout << "processing sub-polygon: " << i << endl;
        cout << *poly << endl;
        cout << "=========================" << endl;
        
        if (i == 4)
        {
            FILE* f = fopen("poly.bin", "wb");
            BOOST_FOREACH( Vertex v, poly->data())
            {
                int32_t i = (int32_t)v.get_x();
                fwrite( &i, sizeof(i), 1, f);
                i = (int32_t)v.get_y();
                fwrite( &i, sizeof(i), 1, f);
            }
            fclose(f);
            
            //cout << c << endl;
        }
        */
        
        
        assert(poly->size() > 3);
        assert(poly->data().front() == poly->data().back());
        if (poly->size() == 4) // edge case: is a triangle (with duplicte end vertex), no further subdivision necessary
        {
            res.push_back( *poly);
            continue;
        }

        //invert x values so that the next execution of createEndMonotoneDiagonals() eliminates MERGE instead of SPLIT vertices
        poly->mirrorX();
        poly->reverse(); // mirroring also inverts the orientation, but this needs to be clockwise for createEndMonotoneDiagonals();
        assert(poly->isClockwise());

        //cout << poly->data() << endl;
        //cout << "===========================" << endl;

        //createCairoDebugOutput( *poly,  dummy, true);
        
        list<LineSegment> diags = createEndMonotoneDiagonals( *poly );
        list<VertexChain> tmp = polygonsFromEndMonotoneGraph(toGraph(*poly, diags), getEndVertices(*poly));

        for ( list<VertexChain>::iterator it = tmp.begin(); it != tmp.end(); it++)
        {
            assert(it->size() > 3); //algorithm should not be able to create degenerate polygons

            it->mirrorX();
            poly->reverse();
            res.push_back( *it );
        }
        
    }
    //cairo_render_polygons(res);
    return res;
}

//typedef int32_t Triangle[6]; //x/y for three vertices



void addTriangle(vector<int32_t> &triangles_out, Vertex v1, Vertex v2, Vertex v3)
{
    triangles_out.push_back( (int32_t)v1.get_x() );
    triangles_out.push_back( (int32_t)v1.get_y() );
    triangles_out.push_back( (int32_t)v2.get_x() );
    triangles_out.push_back( (int32_t)v2.get_y() );
    triangles_out.push_back( (int32_t)v3.get_x() );
    triangles_out.push_back( (int32_t)v3.get_y() );
}

struct TriangulationVertex
{
    TriangulationVertex(Vertex p_Vertex, bool p_onTopChain): vertex(p_Vertex), onTopChain(p_onTopChain) {}

    Vertex vertex;
    bool onTopChain;
};

ostream& operator<<(ostream &os, TriangulationVertex v)
{
    os << (v.onTopChain? "top": "bottom") << ": " << v.vertex;
    return os;
}

static void triangulateMonotoneChains( vector<Vertex> &topChain, vector<Vertex> &bottomChain, vector<int32_t> &res)
{
    //cout << topChain.size() << ";" << bottomChain.size() << endl;
    /*cout << "topChain:" << endl;
    for (uint64_t i = 0; i < topChain.size(); i++)
        cout << topChain[i] << endl;
        
    cout << "bottomChain:" << endl;
    for (uint64_t i = 0; i < bottomChain.size(); i++)
        cout << bottomChain[i] << endl;*/
    
    //vector<int32_t> res;
    assert(topChain.front() == bottomChain.front() );
    assert(topChain.back() == bottomChain.back() );
    assert(topChain.size() >= 2 && bottomChain.size() >= 2);
    
    vector<TriangulationVertex> stack;  //is mostly used as a stack, but we need access to the up to three most recent elements
    //cout << "pushing start vertex " << topChain[0] << endl;
    stack.push_back( TriangulationVertex( topChain[0], true));    // == bottomChain[0]
    Vertex final = topChain.back(); //==botomChain.back(); must be handled individually, because it belongs to both chains at the same time
    topChain.pop_back();
    bottomChain.pop_back();
    assert( topChain[1] != bottomChain[1]);
    uint64_t i1 = 1;
    uint64_t i2 = 1;
    
    if (topChain[1].get_x() > bottomChain[1].get_x())
        stack.push_back( TriangulationVertex( topChain[i1++], true));
    else
        stack.push_back( TriangulationVertex( bottomChain[i2++], false));
    //cout << "pushing " << stack.back() << endl;

    //cout << stack[0] << endl;
    //cout << stack[1] << endl;
    assert( ((Vertex)topChain[1]).isRightOfLine(bottomChain[0], bottomChain[1]) );
    
    while ( i1 != topChain.size() || i2 != bottomChain.size())
    {
        bool next1 = (i2 == bottomChain.size()) || (( i1 != topChain.size()) && (topChain[i1].get_x() > bottomChain[i2].get_x()) );
        TriangulationVertex vNext = next1 ? TriangulationVertex(topChain[i1++], true) : TriangulationVertex(bottomChain[i2++], false);

        assert(stack.size() >= 2);
        if ( stack.back().onTopChain == vNext.onTopChain)
        {
            //cout << "pushing " << vNext << ", stack size is " << stack.size() << endl;
            stack.push_back( vNext );
            while ( stack.size() >= 3)
            {
                uint64_t num = stack.size();
                assert( stack.back().onTopChain == stack[num-2].onTopChain);
                bool onTop = stack.back().onTopChain;
                int dist = stack[num-3].vertex.atSideOfLine(stack[num-2].vertex, stack[num-1].vertex);
                
                if (dist == 0) 
                {
                    /* last two elements from one side, and last element from other side are colinear --> would form a degenerate triangle
                     * instead, just drop that triangle (remove the last but one element from the first side) and continue as if the 
                     * triangle had been output. */
                    stack[num-2] = stack[num-1];
                    stack.pop_back();
                    //cout << "\t" << stack[num-3] << "\n\t" << stack[num-2] << "\n\t" << stack[num-1] << endl;
                    //assert(dist != 0 && "colinear vertices");
                } else
                if ( (onTop && dist < 0)|| (!onTop && dist > 0) ) // form a triangle inside the polygon --> output the triangle and remove it from the stack
                {
                    addTriangle( res, stack[num-3].vertex, stack[num-2].vertex, stack[num-1].vertex);
                    stack[num-2] = stack[num-1];
                    stack.pop_back();
                } 
                else break;
            }
        } else
        {
            for (uint64_t j = 0; j < stack.size()-1; j++)
                addTriangle( res, stack[j].vertex, stack[j+1].vertex, vNext.vertex);

            TriangulationVertex topOfStack = stack.back();
            stack.clear();
            //cout << " === clearing stack === " << endl;
            //cout << "pushing " << topOfStack << endl;
            //cout << "pushing " << vNext << endl;
            stack.push_back(topOfStack);
            stack.push_back(vNext);   
        }
        
    }

    #warning validate this code
    for (uint64_t j = 0; j < stack.size()-1; j++)
        addTriangle( res, stack[j].vertex, stack[j+1].vertex, final);
    
    //cout << "==============" << endl;    
    //cout << "stack size: " << stack.size() << endl;
    //#warning TODO: handle vertex 'final'
//    return res;

}


void triangulateMonotonePolygon( const VertexChain &monotone_polygon, vector<int32_t> &res)
{

    assert(monotone_polygon.data().front() == monotone_polygon.data().back() );
    const vector<Vertex> &poly = monotone_polygon.data();

    if (poly.size() == 4)   //edge case: already a triangle
    {
        addTriangle(res, poly[0], poly[1], poly[2]);
        return;
    }

    
    vector<Vertex> chain1;
    Vertex prev = poly[0];
    int64_t i = 0;
    while (i < (int64_t)poly.size() && poly[i].get_x() <= prev.get_x())
    {
        prev = poly[i++];
        chain1.push_back(prev);
    }
    
    vector<Vertex> chain2;
    int64_t j =  poly.size()-1;
    prev = poly[j];
    while (j>= 0 && poly[j].get_x() <= prev.get_x())
    {
        prev = poly[j--];
        chain2.push_back(prev);
    }
    // Both must have passed the vertex with smallest x-value, and moved one step beyond it (in their respective direction)
    assert( j < i); 
    
    /* Chains only end when the x-coordinates are no longer monotone decreasing.
     * If the two final vertices have identical x-values, then both chains contain
     * both vertices, but in inverse order, instead of ending at the same vertex.
     * This edge case code sorts that out*/
    if (chain1[chain1.size()-1].get_x() == chain1[chain1.size()-2].get_x())
    {
        assert (chain2[chain2.size()-1].get_x() == chain2[chain2.size()-2].get_x());
        assert (chain2[chain2.size()-1].get_x() == chain1[chain1.size()-2].get_x());
        assert (chain1[chain1.size()-1].get_x() == chain2[chain2.size()-2].get_x());
        chain2.pop_back();
    }
    assert( chain1.back() == chain2.back() );

    return triangulateMonotoneChains(chain1, chain2, res);
}

void triangulate(  VertexChain &c, vector<int32_t> &triangles_out)
{
    assert(c.isClockwise());
/*    double t = 0;
    if (c.size() > 50000)
    {
        t = getWallTime();
        cout << "triangulating a polygon with " << c.size() << " vertices" << endl;
    }
*/
    list<VertexChain> polys = toMonotonePolygons (c);
    BOOST_FOREACH( const VertexChain &chain, polys)
        triangulateMonotonePolygon(chain, triangles_out);
  /*      
    if (c.size() > 50000)
        cout << "\t ... took " << (getWallTime() - t)  << " seconds" << endl;
*/
}


vector<int32_t> triangulate( VertexChain &c)
{
    vector<int32_t> res;
    triangulate(c, res);
    return res;
}


