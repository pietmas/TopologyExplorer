#pragma once
#include <TopoDS_Shape.hxx>
#include <map>
#include <vector>

namespace topo
{

enum class FaceFate
{
    Kept,
    Modified,
    Deleted,
    Lost
};

struct FaceTrack
{
    FaceFate fate;
    std::vector<int> newIds;
};

struct DefeatureResult
{
    bool ok = false;
    TopoDS_Shape shape;
    std::map<int, FaceTrack> faceMap;
};

DefeatureResult defeature(const TopoDS_Shape &shape, const std::vector<int> &facesToRemove);

} // namespace topo
