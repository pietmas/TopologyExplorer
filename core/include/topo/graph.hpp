#pragma once
#include <TopoDS_Shape.hxx>
#include <array>
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
    std::array<double, 3> center;
};

struct EdgeArc
{
    int edgeId;
    int faceA;
    int faceB;
    EdgeKind kind;
    double angleDeg;
    double angleTolDeg;
};

struct FaceGraph
{
    std::vector<FaceNode> faces;
    std::vector<EdgeArc> arcs;
    int vertexCount = 0;
    int edgeCount = 0;
    int faceCount = 0;
    int eulerCharacteristic = 0;
    int shellCount = 0;
    int genus = -1;
    int freeEdgeCount = 0;
    int nonManifoldEdgeCount = 0;
    bool repaired = false;
};

TopoDS_Shape loadBrepString(const std::string &data, bool *repaired = nullptr);
std::string writeBrepString(const TopoDS_Shape &shape);
FaceGraph buildFaceGraph(const TopoDS_Shape &shape);

} // namespace topo
