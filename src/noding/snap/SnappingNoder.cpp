/**********************************************************************
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2020 Paul Ramsey <pramsey@cleverelephant.ca>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 **********************************************************************/

#include <geos/noding/MCIndexNoder.h>
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateArraySequence.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/index/kdtree/KdTree.h>
#include <geos/index/kdtree/KdNode.h>
#include <geos/index/kdtree/KdNodeVisitor.h>
#include <geos/noding/SegmentString.h>
#include <geos/noding/NodedSegmentString.h>
#include <geos/noding/snap/SnappingNoder.h>
#include <geos/noding/snap/SnappingIntersectionAdder.h>

//#include <geos/profiler.h>

#include <algorithm> // for std::min and std::max
#include <memory>

using namespace geos::geom;
using namespace geos::index::kdtree;

namespace geos {
namespace noding {  // geos.noding
namespace snap {    // geos.noding.snap


/*public*/
void
SnappingNoder::computeNodes(std::vector<SegmentString*>* inputSegStrings)
{
    std::vector<SegmentString*> snappedSS;
    snapVertices(*inputSegStrings, snappedSS);
    auto result = snapIntersections(snappedSS);
    for (SegmentString* ss: snappedSS) {
        delete ss;
    }
    nodedResult = result.release();
}


/*private*/
void
SnappingNoder::snapVertices(std::vector<SegmentString*>& segStrings, std::vector<SegmentString*>& nodedStrings)
{
    //geos::util::Profiler* profiler = geos::util::Profiler::instance();
    //auto sw = profiler->get(std::string("SnappingNoder::snapVertices"));
    //sw->start();

    seedSnapIndex(segStrings);
    for (SegmentString* ss: segStrings) {
        nodedStrings.push_back(snapVertices(ss));
    }

    //sw->stop();
    //std::cout << sw->name << " - " << sw->getTotFormatted() << std::endl;
}


/*private*/
void
SnappingNoder::seedSnapIndex(std::vector<SegmentString*>& segStrings)
{
    double PHI_INV = (sqrt(5) - 1.0) / 2.0;

    for (SegmentString* ss: segStrings) {
        CoordinateSequence* cs = ss->getCoordinates();
        int numPts = (int) cs->size();
        int numPtsToLoad = numPts / 100;
        double rand = 0.0;

        for (int i = 0; i < numPtsToLoad; i++) {
            // quasi-random sequennce generated by additive recurrence with irrational constant
            rand = rand + PHI_INV;
            if (rand > 1) rand = rand - floor(rand);

            unsigned int index = (unsigned int) (numPts * rand);
            snapIndex.snap(cs->getAt(index));
        }
    }
}

/*private*/
SegmentString*
SnappingNoder::snapVertices(SegmentString* ss)
{
    std::unique_ptr<std::vector<Coordinate>> snapCoords = snap(ss->getCoordinates());
    std::unique_ptr<CoordinateArraySequence> cs(new CoordinateArraySequence(snapCoords.release()));
    return new NodedSegmentString(cs.release(), ss->getData());
}


/*private*/
std::unique_ptr<std::vector<Coordinate>>
SnappingNoder::snap(CoordinateSequence* cs)
{
    std::unique_ptr<std::vector<Coordinate>> snapCoords(new std::vector<Coordinate>);
    for (std::size_t i = 0, sz = cs->size(); i < sz; i++) {
        const Coordinate& pt = snapIndex.snap(cs->getAt(i));
        snapCoords->push_back(pt);
    }
    // Remove repeated points
    snapCoords->erase(std::unique(snapCoords->begin(), snapCoords->end()), snapCoords->end());
    return snapCoords;
}


/*private*/
std::unique_ptr<std::vector<SegmentString*>>
SnappingNoder::snapIntersections(std::vector<SegmentString*>& inputSS)
{
    SnappingIntersectionAdder intAdder(snapTolerance, snapIndex);
    /**
     * Use an overlap tolerance to ensure all
     * possible snapped intersections are found
     */
    MCIndexNoder noder(&intAdder, 2 * snapTolerance);
    noder.computeNodes(&inputSS);
    std::unique_ptr<std::vector<SegmentString*>> result(noder.getNodedSubstrings());
    return result;
}

/*public*/
std::vector<SegmentString*>*
SnappingNoder::getNodedSubstrings() const
{
    return nodedResult;
}


} // namespace geos.noding.snap
} // namespace geos.noding
} // namespace geos