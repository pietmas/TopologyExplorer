#include "topo/features.hpp"
#include "topo/graph.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(topoexplorer, m)
{
    py::enum_<topo::EdgeKind>(m, "EdgeKind")
        .value("Convex", topo::EdgeKind::Convex)
        .value("Concave", topo::EdgeKind::Concave)
        .value("Smooth", topo::EdgeKind::Smooth)
        .value("Unknown", topo::EdgeKind::Unknown);

    py::class_<topo::FaceNode>(m, "FaceNode")
        .def_readonly("id", &topo::FaceNode::id)
        .def_readonly("surface_type", &topo::FaceNode::surfaceType)
        .def_readonly("area", &topo::FaceNode::area);

    py::class_<topo::EdgeArc>(m, "EdgeArc")
        .def_readonly("edge_id", &topo::EdgeArc::edgeId)
        .def_readonly("face_a", &topo::EdgeArc::faceA)
        .def_readonly("face_b", &topo::EdgeArc::faceB)
        .def_readonly("kind", &topo::EdgeArc::kind)
        .def_readonly("angle_deg", &topo::EdgeArc::angleDeg);

    py::class_<topo::FaceGraph>(m, "FaceGraph")
        .def_readonly("faces", &topo::FaceGraph::faces)
        .def_readonly("arcs", &topo::FaceGraph::arcs)
        .def_readonly("vertex_count", &topo::FaceGraph::vertexCount)
        .def_readonly("edge_count", &topo::FaceGraph::edgeCount)
        .def_readonly("face_count", &topo::FaceGraph::faceCount)
        .def_readonly("euler", &topo::FaceGraph::eulerCharacteristic);

    py::enum_<topo::FeatureType>(m, "FeatureType")
        .value("Hole", topo::FeatureType::Hole)
        .value("Fillet", topo::FeatureType::Fillet)
        .value("Pocket", topo::FeatureType::Pocket)
        .value("Boss", topo::FeatureType::Boss);

    py::class_<topo::Feature>(m, "Feature")
        .def_readonly("type", &topo::Feature::type)
        .def_readonly("face_ids", &topo::Feature::faceIds)
        .def_readonly("radius", &topo::Feature::radius)
        .def_readonly("convex", &topo::Feature::convex);

    m.def(
        "recognize",
        [](const std::string &brep)
        {
            TopoDS_Shape shape = topo::loadBrepString(brep);
            return topo::recognizeFeatures(shape, topo::buildFaceGraph(shape));
        }
    );

    m.def(
        "analyze",
        [](const std::string &brep) { return topo::buildFaceGraph(topo::loadBrepString(brep)); }
    );
}
