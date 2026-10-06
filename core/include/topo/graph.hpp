#pragma once
#include <TopoDS_Shape.hxx>
#include <string>
#include <vector>

namespace topo
{

enum class EdgeKind
{
    Convex,
    Concave,
    Smooth,
    Unknown
};

struct FaceNode
{
    int id;
    std::string surfaceType;
    double area;
};

struct EdgeArc
{
    int edgeId;
    int faceA;
    int faceB;
    EdgeKind kind;
    double angleDeg;
};

struct FaceGraph
{
    std::vector<FaceNode> faces;
    std::vector<EdgeArc> arcs;
    int vertexCount = 0;
    int edgeCount = 0;
    int faceCount = 0;
    int eulerCharacteristic = 0;
};

TopoDS_Shape loadBrepString(const std::string &data);
FaceGraph buildFaceGraph(const TopoDS_Shape &shape);

} // namespace topo
