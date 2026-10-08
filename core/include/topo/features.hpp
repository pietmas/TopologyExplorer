#pragma once
#include "topo/graph.hpp"

namespace topo
{

enum class FeatureType
{
    Hole,
    Fillet,
    Pocket,
    Boss
};

struct Feature
{
    FeatureType type;
    std::vector<int> faceIds;
    double radius = 0.0;
    bool convex = false;
};

std::vector<Feature> recognizeFeatures(const TopoDS_Shape &shape, const FaceGraph &g);

} // namespace topo
