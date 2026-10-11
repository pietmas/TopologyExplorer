#include "topo/defeature.hpp"
#include <BRepAlgoAPI_Defeaturing.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>

namespace topo
{

DefeatureResult defeature(const TopoDS_Shape &shape, const std::vector<int> &facesToRemove)
{
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);

    BRepAlgoAPI_Defeaturing d;
    d.SetShape(shape);
    for (int id : facesToRemove)
    {
        d.AddFaceToRemove(faces(id));
    }
    d.SetRunParallel(true);
    d.SetToFillHistory(true);
    d.Build();

    DefeatureResult r;
    r.ok = d.IsDone() && !d.HasWarnings();
    if (!r.ok)
    {
        return r;
    }
    r.shape = d.Shape();

    TopTools_IndexedMapOfShape newFaces;
    TopExp::MapShapes(r.shape, TopAbs_FACE, newFaces);
    for (int i = 1; i <= faces.Extent(); ++i)
    {
        FaceTrack &t = r.faceMap[i];
        if (d.IsDeleted(faces(i)))
        {
            t.fate = FaceFate::Deleted;
            continue;
        }
        const TopTools_ListOfShape &mods = d.Modified(faces(i));
        if (mods.IsEmpty())
        {
            int same = newFaces.FindIndex(faces(i));
            t.fate = same > 0 ? FaceFate::Kept : FaceFate::Lost;
            if (same > 0)
            {
                t.newIds.push_back(same);
            }
            continue;
        }
        t.fate = FaceFate::Modified;
        for (const TopoDS_Shape &m : mods)
        {
            t.newIds.push_back(newFaces.FindIndex(m));
        }
    }
    return r;
}

} // namespace topo
