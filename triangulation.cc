
#include "triangulation.h"

#include "vertexchain.h"
#include <boost/foreach.hpp>


#include "config.h"
#include "geometric_types.h"
#include "vertexchain.h"
#include "helpers.h"

#include <list>


SimpEvent::SimpEvent( const vector<Vertex> &vertices, uint64_t vertex_id, EventType p_type): 
        pos(  vertices[  vertex_id]), 
        pred( vertices[ (vertex_id + vertices.size() -1) % vertices.size() ]), 
        succ( vertices[ (vertex_id                  + 1) % vertices.size() ]),
        type(p_type)
{
    assert( (vertices.front() != vertices.back()) && "For this algorithm, the duplicate back vertex must have been removed");
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
    }
    os << " " << ev.pred <<" / *" << ev.pos << "* / " << ev.succ;
    return os;
}



// ===========================================================

/** returns whether the line 'a' has a smaller or equal y-value at xPos than the line 'b'.
    The method silently assumes that both segments actually intersect the vertical line at xPos.
    Do not make this a method os the LineSegment class, as it has nothing to do with it semantically
  */
bool leq(const LineSegment a, const LineSegment b, BigInt xPos)
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

bool eq(const LineSegment a, const LineSegment b, BigInt xPos)
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
bool leq(const LineSegment a, BigInt xPos, BigInt yPos)
{
    BigInt dax = a.end.get_x() - a.start.get_x();
    assert ((( a.start.get_x() >= xPos && a.end.get_x()  <= xPos) ||
             ( a.end.get_x()   >= xPos && a.start.get_x()<= xPos)) &&
             "position not inside x range of segment");
             
    assert (dax != 0 && "edge case vertical line segment");
    
    bool flipSign = (dax < 0);
    
    BigInt left = (xPos - a.start.get_x()) * ( a.end.get_y() - a.start.get_y() );
    BigInt right= (yPos - a.start.get_y()) * dax;
    
    return (left == right) || ((left < right) != flipSign);
}

bool eq(const LineSegment a, BigInt xPos, BigInt yPos)
{
    BigInt dax = a.end.get_x() - a.start.get_x();
    assert ((( a.start.get_x() >= xPos && a.end.get_x()  <= xPos) ||
             ( a.end.get_x()   >= xPos && a.start.get_x()<= xPos)) &&
             "position not inside x range of segment");
             
    assert (dax != 0 && "edge case vertical line segment");
    
    BigInt left = (xPos - a.start.get_x()) * ( a.end.get_y() - a.start.get_y() );
    BigInt right= (yPos - a.start.get_y()) * dax;
    
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
        if ( leq(e->m_Data, pos.get_x(), pos.get_y()) )
            return e->m_Data;
        
        assert( hasPredecessor(e) );
        LineSegment edge = getPredecessor( e );
        assert( leq(edge, pos.get_x(), pos.get_y()));
        return edge;
    }
    
    void remove(LineSegment item, const BigInt xPos)
    {
        //cout << "removing edge " << item << endl;
    
        assert( item.start.get_x() <= item.end.get_x());
#ifndef NDEBUG
        #warning expensive debug checks (that change the time complexity)
        for (const_iterator it = this->begin(); it != end(); it++)
            assert( it->start.get_x() <= xPos && it->end.get_x() >= xPos && "line arrangement contains outdated edge");
#endif

        AVLTreeNode<LineSegment> *node = findPos(item, xPos);
        assert( node && "Node to be removed does not exist");
        assert( node->m_Data == item);
        AVLTree<LineSegment>::remove(node);
    }
    
    EdgeContainer findPos(LineSegment item, const BigInt xPos)
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
    EdgeContainer findAdjacentEdge(const BigInt xPos, const BigInt yPos)
    {
    	if (! m_pRoot) return NULL;
    	
        EdgeContainer parent=m_pRoot;
	    EdgeContainer pPos = m_pRoot;
	    while (pPos)
	    {
	        parent = pPos;
	        if ( eq(pPos->m_Data,xPos, yPos) ) return pPos;
	        pPos = leq(pPos->m_Data, xPos, yPos) ? pPos->m_pRight : pPos->m_pLeft;
	    }
	    return parent;
    }

    EdgeContainer findAdjacentEdge(Vertex v)
    {
        return findAdjacentEdge( v.get_x(), v.get_y() );
    }
    
    EdgeContainer insert(const LineSegment &item, BigInt xPos)
    {
        //cout << "inserting edge " << item << endl;
        
        // Must not be equal because LineArrangement internally sorts these lines by x-coordinate
        // and a segment with identical x-values at its entpoints does not have a unique x coordinate.
        // Must not be bigger, because our canonical form for status edges is define as having a start
        // with a smaller x-coordinate than the end. This reduced the numbers of necessary checks elsewhere
        assert( item.start.get_x() < item.end.get_x());
    
#ifndef NDEBUG
        #warning expensive debug checks (that change the time complexity)
        for (LineArrangement::const_iterator it = begin(); it != end(); it++)
            assert( it->start.get_x() <= xPos && it->end.get_x() >= xPos && "line arrangement contains outdated edge");
#endif
    
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
    EdgeContainer findParent(const LineSegment &item, BigInt xPos) const
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
        
        BigInt dist = v2.pseudoDistanceToLine(*it, v1);
        assert(dist != 0 && "colinear Vertices");
        
        Vertex top =  dist > 0 ? v2 : v1;
        Vertex bottom=dist > 0 ? v1 : v2;

        cout << "starting at vertex " << *it << ", top: " << top << ", bottom: " << bottom << endl;
        
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
            //bool hasSucc = false;

            //Vertex vv(553921153, 128774301);
            //cout << "nNeighbors " << graph[vv].size() << endl;
            if (graph[v].size() == 2)
            {
                Vertex next1 = graph[v][0];
                Vertex next2 = graph[v][1];
                assert( (next1 == pred) != (next2 == pred)); //exactly one of the connected vertices has to be the predecessor
                vSucc = (next1 == pred) ? next2 : next1;
            } else
            {
                //vector<Vertex> &candidates = ;

                if (updateTop)
                    vSucc = getLeftMostContinuation(graph[v], pred, v);
                else
                    vSucc = getRightMostContinuation(graph[v], pred, v);
                /*
                for ( vector<Vertex>::const_iterator it = candidates.begin(); it != candidates.end(); it++)
                {
                    if (vv == v)
                        cout << "candidate " << *it << endl;
                    if (*it == pred) continue;
                    //if (it->get_x() > v.get_x()) continue; // otherwise polygon would get a second END-vertex, but each polygon can only have one
                    
                    if (!hasSucc)
                    {
                        hasSucc = true;
                        vSucc = *it;
                        continue;
                    }
                    
                    BigInt dist = it->pseudoDistanceToLine(v , vSucc);
                    if (!updateTop) dist = -dist;   //top vertex chain needs leftmost continuation, bottom chain needs rightmost continuation
                    
                    if (dist < 0)
                    {
                        vSucc = *it;
                    } else if (dist == 0)   //edge case: current vertex and two possible successors lie on a single line
                    {
                        assert( (vSucc.get_x() == v.get_x()) && (it->get_x() == v.get_x()));
                        
                        if ( (updateTop && (it->get_y() < vSucc.get_y())) ||
                            (!updateTop && (it->get_y() > vSucc.get_y())))
                            vSucc = *it;
                    }
                }*/
            }
            //assert(hasSucc);
            //cout << "\t\tfound "<< vSucc << endl;
            assert(*it != v);
            if (updateTop)
                top = vSucc;
            else
                bottom= vSucc;                
            
        }
        chain.push_back(top);   //add the final vertex, top == bottom
        chain.push_front(top);  //... and duplicate it to mark it as a closed polygon
        
        cout << "\tclosed polygon with vertex " << top << endl;
        VertexChain c(chain);
        c.canonicalize();
        //std::cout << c.isClockwise() << endl;
        //std::cout << c.data() << endl;
        assert(c.isClockwise());
        res.push_back(c);        
    }
    return res;
}


list<LineSegment> createEndMonotoneDiagonals( const VertexChain &chain)
{
    assert( chain.data().front() == chain.data().back());
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
                assert ( ev.succ.pseudoDistanceToLine(ev.pred, ev.pos) > 0);
                status.insert( LineSegment(ev.pos, ev.pred), ev.pos.get_x() );
                helpers.insert( pair<LineSegment, Vertex>( LineSegment(ev.pos, ev.pred), ev.pos));
                break;
            }
            case END: 
            {
                EdgeContainer e = status.findAdjacentEdge(ev.pos.get_x(), ev.pos.get_y());
#ifndef NDEBUG
                if (ev.succ.get_x() == ev.pos.get_x())
                    /* in this edge case the status edge would have to be a horizontal one, which the status BBST cannot deal with.
                     * As a work-around, the horizontal edge was therefore replaced by a dummy edge that passes through the vertex
                     * that caused the previous event (the successor in clockwise vertex order) */
                    assert( e->m_Data.start == ev.succ || e->m_Data.end == ev.succ);
                else
                    assert( e->m_Data.start == ev.pos  || e->m_Data.end == ev.pos);
#endif
                helpers.erase( e->m_Data);
                status.remove( e->m_Data, ev.pos.get_x());
                
                break;
            }
            case SPLIT:
            {
                LineSegment edge = status.getEdgeLeftOf( ev.pos);
                assert( helpers.count( edge ));
                assert(helpers[edge].get_x() <= ev.pos.get_x());
                newDiagonals.push_back( LineSegment( helpers[edge], ev.pos ));
                
                helpers[edge] = ev.pos;
                
                if (ev.pos.get_x() == ev.pred.get_x())  //edge case: status would be a horizontal line, which LineArrangement can't handle
                {
                    //workaround for degenerate case: insert a dummy edge into 'status'
                    //since the next event necessarily has to be the REGULAR event at ev.pred,
                    //the dummy will not be accessed and will be replaced by the REGULAR event
                    LineSegment dummy_edge = LineSegment( Vertex(ev.pos.get_x()-1, ev.pos.get_y()), ev.pos);
                    status.insert( dummy_edge, ev.pos.get_x() );
                    helpers.insert( pair<LineSegment, Vertex>( dummy_edge, Vertex(0,0)) );
                    
                } else
                {
                    assert ( ev.pred.pseudoDistanceToLine(ev.succ, ev.pos) > 0);
                    status.insert( LineSegment(ev.pos, ev.pred), ev.pos.get_x() );
                    helpers.insert( pair<LineSegment, Vertex>( LineSegment(ev.pos, ev.pred), ev.pos));
                }
                break;
            }
            case MERGE:
            {
                /* A MERGE event merges two ranges to yield one. It occurs at the vertex connecting the two ranges.
                   algorithm: 1. remove the right sub-range from 'status' and 'helpers' (since we now only have a single merged range)
                                 This will be a 'status' edge that (in non-degenerate cases) ends at ev.pos
                              2. set ev.pos as the new helper for the 'status' edge of the left sub-range (now the complete range)
                                 as ev.pos is not the knwon vertex with the highest x-value inside the range */
                assert( ev.succ.pseudoDistanceToLine( ev.pos, ev.pred) > 0);
                
                Vertex incidentAt = ev.succ.get_x() != ev.pos.get_x() ?
                    ev.pos : ev.succ; /*edge case: if ev.pos and ev.succ have a common x-value, then ev.pos will be 
                                                   the MERGE event, but the status edge will be incident to ev.succ */
                
                LineSegment leftEdge = status.getEdgeLeftOf(incidentAt);
                assert( leftEdge.end == incidentAt );
                
                //assert ( status.findPos( leftEdge, ev.pos.get_x() ) );
                status.remove( leftEdge, ev.pos.get_x() );
                assert( helpers.count(leftEdge) );
                helpers.erase( leftEdge);

                LineSegment edge = status.getEdgeLeftOf( ev.pos);
                assert( helpers.count(edge));
                helpers[edge] = ev.pos;
                break;
            }
            case REGULAR: //TODO: replace "REGULAR" event by distinct REGULAR_POLY_LEFT und REGULAR_PLY_RIGHT events.
                
                // ensure that colinearities have been removed (e.g. by a prior call to canonicalize())
                assert(  (ev.pred.get_x() != ev.pos.get_x() || ev.succ.get_x() != ev.pos.get_x()) && "Colinear vertices"); 
                if        (ev.pred.get_x() >= ev.pos.get_x() && ev.succ.get_x() <= ev.pos.get_x())
                {   //pred below succ (and polygon is clockwise) --> polygon must be *right* of REGULAR vertex
                    if ( ev.pred.get_x() == ev.pos.get_x() && ev.succ.get_x() < ev.pos.get_x() )
                    {   //edge case: predecessor has identical x value, but successor does not
                        //resolution: do nothing, status and helper are still valid
                    } else
                    {   
                        Vertex prev_status_end = 
                        /* edge case: predecessor has higher x value, but successor does not --> always happens
                         *            at the vertex right after the previous case, i.e. the status ends at the 'succ' vertex*/
                            ( ev.pred.get_x() > ev.pos.get_x() && ev.succ.get_x() == ev.pos.get_x() ) ?
                            ev.succ : ev.pos;
                            
                        EdgeContainer e = status.findAdjacentEdge( prev_status_end );
                        assert (e && (e->m_Data.end == prev_status_end));
                        assert (helpers.count(e->m_Data));
                        helpers.erase( e->m_Data);
                        status.remove( e->m_Data, prev_status_end.get_x());
                        
                        LineSegment newEdge( ev.pos, ev.pred);
                        status.insert( newEdge, ev.pos.get_x() );
                        helpers.insert( pair<LineSegment, Vertex>( newEdge, ev.pos));
                    }
                } else if (ev.pred.get_x() <= ev.pos.get_x() && ev.succ.get_x() >= ev.pos.get_x() )
                {   //pred above succ (and polygon is clockwise) --> polygon must be *left* of REGULAR vertex
                    //assert (ev.pred.get_x() != ev.pos.get_x() && ev.succ.get_x() != ev.pos.get_x() && "Not Implemented");
                    Vertex newHelper = ( ev.pos.get_x() == ev.succ.get_x()) ? ev.succ : ev.pos;
                    
                    LineSegment edge = status.getEdgeLeftOf( newHelper );
                    
                    assert( helpers.count(edge));
                    helpers[edge] = newHelper;
                
                } else assert(false);
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

list<VertexChain> toMonotonePolygons( const VertexChain &chain)
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
        static int i = 0;
        i++;
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
    
    //vector<int32_t> res;
    assert(topChain.front() == bottomChain.front() );
    assert(topChain.back() == bottomChain.back() );
    assert(topChain.size() >= 2 && bottomChain.size() >= 2);
    
    vector<TriangulationVertex> stack;  //is mostly used as a stack, but we need access to the up to three most recent elements
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

    //cout << stack[0] << endl;
    //cout << stack[1] << endl;
    assert( ((Vertex)topChain[1]).pseudoDistanceToLine(bottomChain[0], bottomChain[1]) > 0);
    
    while ( i1 != topChain.size() || i2 != bottomChain.size())
    {
        bool next1 = (i2 == bottomChain.size()) || (( i1 != topChain.size()) && (topChain[i1].get_x() > bottomChain[i2].get_x()) );
        TriangulationVertex vNext = next1 ? TriangulationVertex(topChain[i1++], true) : TriangulationVertex(bottomChain[i2++], false);

        assert(stack.size() >= 2);
        if ( stack.back().onTopChain == vNext.onTopChain)
        {
            stack.push_back( vNext );
            while ( stack.size() >= 3)
            {
                uint64_t num = stack.size();
                assert( stack[num-1].onTopChain == stack[num-2].onTopChain);
                bool onTop = stack[num-1].onTopChain;
                BigInt dist = stack[num-3].vertex.pseudoDistanceToLine(stack[num-2].vertex, stack[num-1].vertex);
                if (dist == 0)
                    cout << "\t" << stack[num-3] << "\n\t" << stack[num-2] << "\n\t" << stack[num-3] << endl;
                assert(dist != 0 && "colinear vertices");
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

vector<int32_t> triangulate( const VertexChain &c)
{
    double t = getWallTime();
    static int i = 0;
    i++;
    cout << "triangulating polygon " << i << endl;
    if (i == 22)
    {
        FILE* f = fopen("poly.bin", "wb");
        BOOST_FOREACH( Vertex v, c.data())
        {
            int32_t i = (int32_t)v.get_x();
            fwrite( &i, sizeof(i), 1, f);
            i = (int32_t)v.get_y();
            fwrite( &i, sizeof(i), 1, f);
        }
        fclose(f);
        
        cout << c << endl;
    }
    if (c.size() > 100)
        cout << "triangulating a polygon with " << c.size() << " vertices" << endl;

    vector<int32_t> res;
    list<VertexChain> polys = toMonotonePolygons (c.data());
    BOOST_FOREACH( const VertexChain &chain, polys)
        triangulateMonotonePolygon(chain, res);
        
    if (c.size() > 100)
        cout << "\t ... took" << (getWallTime() - t)  << "seconds" << endl;

    return res;
}


