#include "topo/graph.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GProp_GProps.hxx>
#include <Geom2d_Curve.hxx>
#include <Precision.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Vec.hxx>
#include <set>
#include <sstream>
#include <stdexcept>

namespace topo
{

TopoDS_Shape loadBrepString(const std::string &data, bool *repaired)
{
    std::istringstream in(data);
    TopoDS_Shape shape;
    BRep_Builder builder;
    try
    {
        BRepTools::Read(shape, in, builder);
    }
    catch (const Standard_Failure &)
    {
        shape.Nullify();
    }
    if (shape.IsNull())
    {
        throw std::runtime_error("failed to parse BREP");
    }
    bool valid = BRepCheck_Analyzer(shape).IsValid();
    if (repaired)
    {
        *repaired = !valid;
    }
    // Top.brep needs two goes
    for (int pass = 0; pass < 3 && !valid; ++pass)
    {
        ShapeFix_Shape fix(shape);
        fix.Perform();
        shape = fix.Shape();
        valid = !shape.IsNull() && BRepCheck_Analyzer(shape).IsValid();
    }
    if (!valid)
    {
        throw std::runtime_error("invalid shape, ShapeFix couldn't repair it");
    }
    return shape;
}

std::string writeBrepString(const TopoDS_Shape &shape)
{
    std::ostringstream out;
    BRepTools::Write(shape, out);
    return out.str();
}

static const char *surfaceName(GeomAbs_SurfaceType t)
{
    switch (t)
    {
    case GeomAbs_Plane:
        return "Plane";
    case GeomAbs_Cylinder:
        return "Cylinder";
    case GeomAbs_Cone:
        return "Cone";
    case GeomAbs_Sphere:
        return "Sphere";
    case GeomAbs_Torus:
        return "Torus";
    case GeomAbs_BezierSurface:
        return "Bezier";
    case GeomAbs_BSplineSurface:
        return "BSpline";
    case GeomAbs_SurfaceOfRevolution:
        return "Revolution";
    case GeomAbs_SurfaceOfExtrusion:
        return "Extrusion";
    default:
        return "Other";
    }
}

static TopAbs_Orientation edgeOrientationInFace(const TopoDS_Edge &edge, const TopoDS_Face &face)
{
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next())
    {
        if (ex.Current().IsSame(edge))
        {
            return ex.Current().Orientation();
        }
    }
    return TopAbs_EXTERNAL;
}

static bool
faceNormalAtEdge(const TopoDS_Face &face, const TopoDS_Edge &edge, double t, gp_Vec &normal)
{
    double f, l;
    Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(edge, face, f, l);
    if (pc.IsNull())
    {
        return false;
    }
    gp_Pnt2d uv = pc->Value(t);
    BRepAdaptor_Surface s(face);
    BRepLProp_SLProps props(s, uv.X(), uv.Y(), 1, Precision::Confusion());
    if (!props.IsNormalDefined())
    {
        return false;
    }
    normal = gp_Vec(props.Normal());
    if (face.Orientation() == TopAbs_REVERSED)
    {
        normal.Reverse();
    }
    return true;
}

static EdgeKind classifyEdge(
    const TopoDS_Edge &edge,
    const TopoDS_Face &fa,
    const TopoDS_Face &fb,
    double &angleDeg,
    double &tolDeg
)
{
    double tol = BRep_Tool::Tolerance(edge);
    double len = GCPnts_AbscissaPoint::Length(BRepAdaptor_Curve(edge));
    double tolRad = std::max(Precision::Angular(), len > tol ? tol / len : 0.0);
    tolDeg = tolRad * 180.0 / M_PI;

    TopAbs_Orientation oa = edgeOrientationInFace(edge, fa);
    if (oa != TopAbs_FORWARD && oa != TopAbs_REVERSED)
    {
        return EdgeKind::Unknown;
    }

    BRepAdaptor_Curve c(edge);
    double t = 0.5 * (c.FirstParameter() + c.LastParameter());
    gp_Pnt p;
    gp_Vec tangent;
    c.D1(t, p, tangent);
    if (tangent.Magnitude() < Precision::Confusion())
    {
        return EdgeKind::Unknown;
    }
    if (oa == TopAbs_REVERSED)
    {
        tangent.Reverse();
    }

    gp_Vec na, nb;
    if (!faceNormalAtEdge(fa, edge, t, na) || !faceNormalAtEdge(fb, edge, t, nb))
    {
        return EdgeKind::Unknown;
    }

    double dot = na.Normalized().Dot(nb.Normalized());
    angleDeg = std::acos(std::max(-1.0, std::min(1.0, dot))) * 180.0 / M_PI;
    if (angleDeg < tolDeg)
    {
        return EdgeKind::Smooth;
    }

    double s = na.Crossed(nb).Dot(tangent.Normalized());
    return s > 0 ? EdgeKind::Convex : EdgeKind::Concave;
}

FaceGraph buildFaceGraph(const TopoDS_Shape &shape)
{
    TopTools_IndexedMapOfShape faces, edges, verts, wires, shells;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_WIRE, wires);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(shape, TopAbs_VERTEX, verts);
    TopExp::MapShapes(shape, TopAbs_SHELL, shells);

    int degenerate = 0;
    for (int e = 1; e <= edges.Extent(); ++e)
    {
        if (BRep_Tool::Degenerated(TopoDS::Edge(edges(e))))
        {
            degenerate++;
        }
    }

    FaceGraph g;
    g.faceCount = faces.Extent();
    g.edgeCount = edges.Extent() - degenerate;
    g.vertexCount = verts.Extent();
    g.shellCount = shells.Extent();
    int loopCount = wires.Extent();
    g.eulerCharacteristic = g.vertexCount - g.edgeCount + g.faceCount - (loopCount - g.faceCount);

    for (int i = 1; i <= faces.Extent(); ++i)
    {
        const TopoDS_Face &f = TopoDS::Face(faces(i));
        BRepAdaptor_Surface s(f);
        GProp_GProps props;
        BRepGProp::SurfaceProperties(f, props);
        gp_Pnt c = props.CentreOfMass();
        g.faces.push_back({i, surfaceName(s.GetType()), props.Mass(), {c.X(), c.Y(), c.Z()}});
    }

    TopTools_IndexedDataMapOfShapeListOfShape edgeToFaces;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeToFaces);

    for (int e = 1; e <= edgeToFaces.Extent(); ++e)
    {
        const TopoDS_Edge &edge = TopoDS::Edge(edgeToFaces.FindKey(e));
        if (BRep_Tool::Degenerated(edge))
        {
            continue;
        }
        const TopTools_ListOfShape &adj = edgeToFaces(e);

        std::set<int> touching;
        for (const TopoDS_Shape &f : adj)
        {
            touching.insert(faces.FindIndex(f));
        }
        if (touching.size() == 1 && adj.Extent() == 1)
        {
            g.freeEdgeCount++;
        }
        if (touching.size() > 2)
        {
            g.nonManifoldEdgeCount++;
        }
        if (adj.Extent() != 2 || touching.size() != 2)
        {
            continue;
        }

        int fa = faces.FindIndex(adj.First());
        int fb = faces.FindIndex(adj.Last());
        double ang = 0.0;
        double tolDeg = 0.0;
        EdgeKind k =
            classifyEdge(edge, TopoDS::Face(adj.First()), TopoDS::Face(adj.Last()), ang, tolDeg);
        g.arcs.push_back({edges.FindIndex(edge), fa, fb, k, ang, tolDeg});
    }

    bool closed = g.freeEdgeCount == 0 && g.nonManifoldEdgeCount == 0 && g.shellCount > 0;
    if (closed && g.eulerCharacteristic % 2 == 0)
    {
        g.genus = g.shellCount - g.eulerCharacteristic / 2;
    }
    return g;
}

} // namespace topo
