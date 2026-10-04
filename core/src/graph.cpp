#include "topo/graph.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <sstream>
#include <stdexcept>

namespace topo {

TopoDS_Shape loadBrepString(const std::string &data) {
  std::istringstream in(data);
  TopoDS_Shape shape;
  BRep_Builder builder;
  BRepTools::Read(shape, in, builder);
  if (shape.IsNull())
    throw std::runtime_error("failed to parse BREP");
  return shape;
}

static const char *surfaceName(GeomAbs_SurfaceType t) {
  switch (t) {
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

FaceGraph buildFaceGraph(const TopoDS_Shape &shape) {
  TopTools_IndexedMapOfShape faces, edges, verts, wires;
  TopExp::MapShapes(shape, TopAbs_FACE, faces);
  TopExp::MapShapes(shape, TopAbs_WIRE, wires);
  TopExp::MapShapes(shape, TopAbs_EDGE, edges);
  TopExp::MapShapes(shape, TopAbs_VERTEX, verts);

  FaceGraph g;
  g.faceCount = faces.Extent();
  g.edgeCount = edges.Extent();
  g.vertexCount = verts.Extent();
  // Euler-Poincare: V - E + F - (L - F) = 2(S - G). Plain V - E + F misses
  // faces with inner loops (L > F), so it stays 2 for parts with holes.
  int loopCount = wires.Extent();
  g.eulerCharacteristic =
      g.vertexCount - g.edgeCount + g.faceCount - (loopCount - g.faceCount);

  for (int i = 1; i <= faces.Extent(); ++i) {
    const TopoDS_Face &f = TopoDS::Face(faces(i));
    BRepAdaptor_Surface s(f);
    GProp_GProps props;
    BRepGProp::SurfaceProperties(f, props);
    g.faces.push_back({i, surfaceName(s.GetType()), props.Mass()});
  }

  TopTools_IndexedDataMapOfShapeListOfShape edgeToFaces;
  TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeToFaces);

  for (int e = 1; e <= edgeToFaces.Extent(); ++e) {
    const TopTools_ListOfShape &adj = edgeToFaces(e);
    if (adj.Extent() != 2)
      continue; // boundary or non-manifold
    int fa = faces.FindIndex(adj.First());
    int fb = faces.FindIndex(adj.Last());
    if (fa == fb)
      continue; // seam edge on one face
    g.arcs.push_back({edges.FindIndex(edgeToFaces.FindKey(e)), fa, fb,
                      EdgeKind::Unknown, 0.0});
  }
  return g;
}

} // namespace topo
