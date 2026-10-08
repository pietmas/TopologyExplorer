#include "topo/features.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepTools.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Lin.hxx>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace topo
{

static const double tol = 1e-6;

struct FaceInfo
{
    TopoDS_Face face;
    GeomAbs_SurfaceType type;
    std::vector<const EdgeArc *> arcs;
    std::set<int> outerEdges;
    gp_Pnt point;
    gp_Vec normal;
};

static int otherFace(const EdgeArc &a, int f)
{
    if (a.faceA == f)
    {
        return a.faceB;
    }
    return a.faceA;
}

static void outwardNormal(const TopoDS_Face &f, gp_Pnt &p, gp_Vec &n)
{
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    BRepAdaptor_Surface s(f);
    BRepLProp_SLProps props(s, 0.5 * (u0 + u1), 0.5 * (v0 + v1), 1, tol);
    p = props.Value();
    n = gp_Vec();
    if (props.IsNormalDefined())
    {
        n = gp_Vec(props.Normal());
    }
    if (f.Orientation() == TopAbs_REVERSED)
    {
        n.Reverse();
    }
}

static bool pointsInward(const FaceInfo &fi)
{
    BRepAdaptor_Surface s(fi.face);
    gp_Vec radial;
    if (fi.type == GeomAbs_Cylinder)
    {
        gp_Ax1 axis = s.Cylinder().Axis();
        gp_Vec v(axis.Location(), fi.point);
        gp_Vec d(axis.Direction());
        radial = v - d * v.Dot(d);
    }
    else if (fi.type == GeomAbs_Sphere)
    {
        radial = gp_Vec(s.Sphere().Location(), fi.point);
    }
    else if (fi.type == GeomAbs_Torus)
    {
        gp_Torus t = s.Torus();
        gp_Vec v(t.Location(), fi.point);
        gp_Vec d(t.Axis().Direction());
        gp_Vec flat = v - d * v.Dot(d);
        if (flat.Magnitude() < tol)
        {
            return false;
        }
        gp_Pnt tubeCentre = t.Location().Translated(flat.Normalized() * t.MajorRadius());
        radial = gp_Vec(tubeCentre, fi.point);
    }
    else
    {
        return false;
    }
    return fi.normal.Dot(radial) < 0;
}

static double radiusOf(const TopoDS_Face &f)
{
    BRepAdaptor_Surface s(f);
    if (s.GetType() == GeomAbs_Cylinder)
    {
        return s.Cylinder().Radius();
    }
    if (s.GetType() == GeomAbs_Sphere)
    {
        return s.Sphere().Radius();
    }
    if (s.GetType() == GeomAbs_Torus)
    {
        return s.Torus().MinorRadius();
    }
    return 0.0;
}

static double angleAround(const TopoDS_Face &f)
{
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    return u1 - u0;
}

static bool sameCylinder(const TopoDS_Face &a, const TopoDS_Face &b)
{
    gp_Cylinder ca = BRepAdaptor_Surface(a).Cylinder();
    gp_Cylinder cb = BRepAdaptor_Surface(b).Cylinder();
    if (std::abs(ca.Radius() - cb.Radius()) > tol)
    {
        return false;
    }
    if (!ca.Axis().IsParallel(cb.Axis(), tol))
    {
        return false;
    }
    return gp_Lin(ca.Axis()).Distance(cb.Axis().Location()) < tol;
}

static bool isRightAngle(const EdgeArc &a)
{
    return std::abs(a.angleDeg - 90.0) < 1e-3;
}

static int findGroup(std::vector<int> &group, int f)
{
    while (group[f] != f)
    {
        f = group[f];
    }
    return f;
}

static void findHoles(
    std::vector<FaceInfo> &info,
    const FaceGraph &g,
    std::vector<bool> &used,
    std::vector<Feature> &out
)
{
    int n = info.size() - 1;
    std::vector<bool> holeWall(n + 1, false);
    for (int i = 1; i <= n; ++i)
    {
        holeWall[i] = info[i].type == GeomAbs_Cylinder && pointsInward(info[i]);
    }

    std::vector<int> group(n + 1);
    for (int i = 0; i <= n; ++i)
    {
        group[i] = i;
    }
    for (const EdgeArc &a : g.arcs)
    {
        if (a.kind != EdgeKind::Smooth || !holeWall[a.faceA] || !holeWall[a.faceB])
        {
            continue;
        }
        if (sameCylinder(info[a.faceA].face, info[a.faceB].face))
        {
            group[findGroup(group, a.faceA)] = findGroup(group, a.faceB);
        }
    }

    std::map<int, std::vector<int>> walls;
    for (int i = 1; i <= n; ++i)
    {
        if (holeWall[i])
        {
            walls[findGroup(group, i)].push_back(i);
        }
    }

    for (auto &entry : walls)
    {
        std::vector<int> &faces = entry.second;
        double total = 0.0;
        for (int f : faces)
        {
            total += angleAround(info[f].face);
        }
        if (total < 2 * M_PI - tol)
        {
            continue;
        }

        gp_Cylinder cyl = BRepAdaptor_Surface(info[faces[0]].face).Cylinder();
        Feature hole{FeatureType::Hole, faces, cyl.Radius()};
        std::set<int> inHole(faces.begin(), faces.end());

        for (int f : faces)
        {
            used[f] = true;
            for (const EdgeArc *a : info[f].arcs)
            {
                int floor = otherFace(*a, f);
                const FaceInfo &fl = info[floor];
                if (a->kind != EdgeKind::Concave || fl.type != GeomAbs_Plane || used[floor])
                {
                    continue;
                }
                if (!gp_Vec(cyl.Axis().Direction()).IsParallel(fl.normal, tol))
                {
                    continue;
                }
                bool onlyTouchesHole = true;
                for (const EdgeArc *b : fl.arcs)
                {
                    if (inHole.count(otherFace(*b, floor)) == 0)
                    {
                        onlyTouchesHole = false;
                    }
                }
                if (onlyTouchesHole)
                {
                    hole.faceIds.push_back(floor);
                    used[floor] = true;
                }
            }
        }
        std::sort(hole.faceIds.begin(), hole.faceIds.end());
        out.push_back(hole);
    }
}

static void
findFillets(std::vector<FaceInfo> &info, std::vector<bool> &used, std::vector<Feature> &out)
{
    int n = info.size() - 1;
    for (int i = 1; i <= n; ++i)
    {
        const FaceInfo &fi = info[i];
        if (used[i])
        {
            continue;
        }
        if (fi.type != GeomAbs_Cylinder && fi.type != GeomAbs_Torus && fi.type != GeomAbs_Sphere)
        {
            continue;
        }
        if (fi.type == GeomAbs_Cylinder && angleAround(fi.face) >= 2 * M_PI - tol)
        {
            continue;
        }

        int smooth = 0;
        for (const EdgeArc *a : fi.arcs)
        {
            if (a->kind != EdgeKind::Smooth)
            {
                continue;
            }
            const FaceInfo &other = info[otherFace(*a, i)];
            if (fi.type == GeomAbs_Cylinder && other.type == GeomAbs_Cylinder &&
                sameCylinder(fi.face, other.face))
            {
                continue;
            }
            smooth++;
        }
        if (smooth >= 2)
        {
            out.push_back({FeatureType::Fillet, {i}, radiusOf(fi.face), !pointsInward(fi)});
        }
    }
}

static bool hasConcaveBase(const FaceInfo &wall, int wallId, int top)
{
    for (const EdgeArc *a : wall.arcs)
    {
        if (a->kind == EdgeKind::Concave && otherFace(*a, wallId) != top)
        {
            return true;
        }
    }
    return false;
}

static void findPocketsAndBosses(
    std::vector<FaceInfo> &info,
    std::vector<bool> &used,
    std::vector<Feature> &out
)
{
    int n = info.size() - 1;
    for (int i = 1; i <= n; ++i)
    {
        const FaceInfo &fi = info[i];
        if (fi.type != GeomAbs_Plane || used[i])
        {
            continue;
        }

        int rimArcs = 0;
        int concave = 0;
        int convex = 0;
        std::vector<int> walls;
        for (const EdgeArc *a : fi.arcs)
        {
            if (fi.outerEdges.count(a->edgeId) == 0)
            {
                continue;
            }
            rimArcs++;
            if (a->kind == EdgeKind::Concave && isRightAngle(*a))
            {
                concave++;
            }
            if (a->kind == EdgeKind::Convex && isRightAngle(*a))
            {
                convex++;
            }
            int w = otherFace(*a, i);
            if (std::find(walls.begin(), walls.end(), w) == walls.end())
            {
                walls.push_back(w);
            }
        }
        if (walls.empty())
        {
            continue;
        }

        std::sort(walls.begin(), walls.end());
        std::vector<int> ids{i};
        ids.insert(ids.end(), walls.begin(), walls.end());

        if (concave == rimArcs)
        {
            out.push_back({FeatureType::Pocket, ids});
            continue;
        }
        if (convex != rimArcs)
        {
            continue;
        }
        bool isBoss = true;
        for (int w : walls)
        {
            if (!hasConcaveBase(info[w], w, i))
            {
                isBoss = false;
            }
        }
        if (isBoss)
        {
            out.push_back({FeatureType::Boss, ids, 0.0, true});
        }
    }
}

std::vector<Feature> recognizeFeatures(const TopoDS_Shape &shape, const FaceGraph &g)
{
    TopTools_IndexedMapOfShape faces, edges;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);

    int n = faces.Extent();
    std::vector<FaceInfo> info(n + 1);
    for (int i = 1; i <= n; ++i)
    {
        FaceInfo &fi = info[i];
        fi.face = TopoDS::Face(faces(i));
        fi.type = BRepAdaptor_Surface(fi.face).GetType();
        outwardNormal(fi.face, fi.point, fi.normal);
        TopoDS_Wire outer = BRepTools::OuterWire(fi.face);
        if (outer.IsNull())
        {
            continue;
        }
        for (TopExp_Explorer ex(outer, TopAbs_EDGE); ex.More(); ex.Next())
        {
            fi.outerEdges.insert(edges.FindIndex(ex.Current()));
        }
    }
    for (const EdgeArc &a : g.arcs)
    {
        info[a.faceA].arcs.push_back(&a);
        info[a.faceB].arcs.push_back(&a);
    }

    std::vector<Feature> out;
    std::vector<bool> used(n + 1, false);
    findHoles(info, g, used, out);
    findFillets(info, used, out);
    findPocketsAndBosses(info, used, out);
    return out;
}

} // namespace topo
