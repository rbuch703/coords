
/* this file contains methods not currently in use or incomplete, along with the rationale for removing each bit*/


/** works for the general case, but no edge case is implemented.
    implementing and testing the edge cases is likely to be more time-consuming that implementing a different approach
  **/
list<pair<ActiveEdge,ActiveEdge> > findIntersections(list<ActiveEdge> &edges)
{
    list<pair<ActiveEdge,ActiveEdge> > intersections;

    SimpEventQueue events;

    BOOST_FOREACH( ActiveEdge &edge, edges)
    {
        events.add( SEG_START, edge);
        events.add( SEG_END, edge);
    }

    LineArrangement lines;

    //int num_intersections = 0;
    
    while ( events.containsEvents() )
    {
        SimpEvent event = events.pop();
        
        
        
        switch (event.type)
        {
        case SEG_START:
        {
            /* algorithm: 
                1. insert the segment at the correct position in the line arrangement
                   (so that the lines 'physically' directly to its left and right are also 
                   its neighbors in the line arrangement data structure
                2. if there was a scheduled intersection between its predecessor and successor, remove that event
                3. if it potentially intersects with its predecessor or successor, add the corresponding events
                        
             */
            BigInt x_pos = event.m_thisEdge.left.x;
            BigInt y_pos = event.m_thisEdge.left.y;
            ActiveEdge &edge = event.m_thisEdge;
            
            std::cout << "adding edge " << edge << std::endl;
            AVLTreeNode<ActiveEdge>* node = lines.addEdge( edge, x_pos);
            
            //FIXME: add edge case where the new line segment overlaps (i.e. is colinear) with an older one
            /*FIXME: add edge case that new line is parallel to the sweep line */
            
            ActiveEdge pred, succ;
            if (lines.hasPredecessor(node)) pred = lines.getPredecessor(node);
            if (lines.hasSuccessor(  node)) succ = lines.getSuccessor(  node);

            if (lines.hasPredecessor(node) && lines.hasSuccessor(node))
            {
                assert (pred.isLessThanFuture(succ, x_pos)); // integrity check for the line arrangement
                events.unscheduleIntersection(pred, succ, x_pos, y_pos);
            }
            
            if (lines.hasPredecessor(node))
            {
                assert (pred.isLessThanFuture(edge, x_pos));
                events.scheduleIntersection(pred, edge, x_pos, y_pos);
            }
            
            if (lines.hasSuccessor(node))
            {
                assert (edge.isLessThanFuture(succ, x_pos));
                events.scheduleIntersection(edge, succ, x_pos, y_pos);
            }
            break;
        }
        case SEG_END:
        {
            /* Algorithm:
                * 1. remove segment from line arrangement
                * 2. if its predecessor and successor have a future intersection, add that event to the queue
            */
                ActiveEdge &edge = event.m_thisEdge;
                
                //FIXME: edge case that this segement intersects another segement at this end point
                list<EdgeContainer> nodes = lines.findAllIntersectingEdges(edge, event.x);
                assert(nodes.size() == 1 && "Not implemented");
                
                AVLTreeNode<ActiveEdge> *node = lines.findPos(edge, event.x);
                assert(node);
                
                if (lines.hasPredecessor(node) && lines.hasSuccessor(node))
                    events.scheduleIntersection(lines.getPredecessor(node), lines.getSuccessor(node), event.x, event.y);
                
                lines.remove(edge, event.x);
                std::cout << "removing edge " << edge << std::endl;
            break;
        }
        case INTERSECTION:
        {
            /* algorithm: 1. report intersection
                          FIXME: (make sure no other intersection takes place at the same position
                          2. remove intersection events of the two intersecting edges with their outer neighbors
                          3. swap the two intersectings edge in the LineArrangement
                          4. re-add new intersection events with their new outer neighbors
            */
            assert (event.m_thisEdge.intersects(event.m_otherEdge));
            intersections.push_back( pair<ActiveEdge,ActiveEdge>(event.m_thisEdge, event.m_otherEdge));

            BigFraction int_x, int_y;
            event.m_thisEdge.getIntersectionWith( event.m_otherEdge, int_x, int_y);

            std::cout << "intersection between " << event.m_thisEdge << " and " << event.m_otherEdge << " at (" << int_x << ", " << int_y << " )" << std::endl;
            
            list<EdgeContainer> nodes = lines.findAllIntersectingEdges(event.m_thisEdge, event.x);
            if (nodes.size() != 2)
            {
                std::cout << "### nodes dump" << std::endl;
                BOOST_FOREACH(AVLTreeNode<ActiveEdge>* node, nodes)
                    std::cout << node->m_Data << std::endl;
            }
            assert( nodes.size() == 2 && "Multiple intersections on a single position are not implemented");
            
            EdgeContainer left = nodes.front();
            EdgeContainer right = nodes.back();
            
            if (lines.hasPredecessor(left))
                events.unscheduleIntersection( lines.getPredecessor(left), left->m_Data, event.x, event.y);
            
            if (lines.hasSuccessor(right))
                events.unscheduleIntersection( right->m_Data, lines.getSuccessor(right), event.x, event.y);

            assert ( left->m_Data.isEqual(right->m_Data, event.x));
            
            ActiveEdge tmp = left->m_Data;
            left->m_Data = right->m_Data;
            right->m_Data = tmp;

            if (lines.hasPredecessor(left))
                events.scheduleIntersection( lines.getPredecessor(left), left->m_Data, event.x, event.y);

            if (lines.hasSuccessor(right))
                events.scheduleIntersection( right->m_Data, lines.getSuccessor(right), event.x, event.y);
            
            break;
        }
        default:
            assert(false && "Invalid event type");break;
        }

        assert( lines.isConsistent( event.x ) );
        
    }
    
    assert( lines.size() == 0);
    assert( events.size() == 0);
    return intersections;
}

/* test code for the above*/

void testIntersectionTest()
{
/*    
    static const int32_t NUM_EDGES = 250;
    list<ActiveEdge> edges;
    for (int i = 0; i < NUM_EDGES; i++)
    {
        Vertex v1(rand() % 256, rand() % 256);
        Vertex v2(rand() % 256, rand() % 256);

        edges.push_back( ActiveEdge(v1 < v2 ? v1:v2 , v1<v2? v2:v1) );
        //std::cout << edges[i] << endl;
    }

    list< pair<ActiveEdge,ActiveEdge> > intersections = findIntersections( edges);
    std::cout << "Found " << intersections.size() << " intersections " << std::endl;
    
    int n_ints = 0;
    for (list<ActiveEdge>::const_iterator it = edges.begin(); it != edges.end(); it++)
    {
        list<ActiveEdge>::const_iterator it2 = it;
        
        for (it2++; it2 != edges.end(); it2++)
            if (it->intersects(*it2)) n_ints++;
    }

    std::cout << "Found " << n_ints << " intersections " << std::endl;
*/  

}
