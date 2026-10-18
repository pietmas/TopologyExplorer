"""Look at what the engine thinks, in ocp_vscode / ocp_viewer.

    python viz.py <part> [surface|features|defeature]
"""

import math
import sys

import networkx as nx
from build123d import CenterOf, Location
from ocp_vscode import Camera, show

import parts
import topoexplorer as te
from bridge import from_brep_string, to_brep_string

SURFACE_COLORS = {
    "Plane": "lightgray",
    "Cylinder": "orange",
    "Sphere": "tomato",
    "Torus": "gold",
    "Cone": "violet",
}

FEATURE_COLORS = {
    te.FeatureType.Hole: "crimson",
    te.FeatureType.Fillet: "gold",
    te.FeatureType.Pocket: "royalblue",
    te.FeatureType.Boss: "seagreen",
}

PLAIN = "lightgray"
CHANGED = "orange"


def match_faces(part, g):
    """C++ face id -> build123d face, matched by centre of mass and area."""
    faces = part.faces()
    centers = [tuple(f.center(CenterOf.MASS)) for f in faces]

    matched = {}
    for node in g.faces:
        best, best_miss = None, math.inf
        for j, f in enumerate(faces):
            # mm + mm^2, not pretty, but both are 0 on a real match
            miss = math.dist(centers[j], node.center) + abs(f.area - node.area)
            if miss < best_miss:
                best, best_miss = j, miss
        if best_miss > 1e-6:
            raise ValueError(f"no build123d face sits where face {node.id} does")
        matched[node.id] = best

    if len(set(matched.values())) != len(faces):
        raise ValueError("two C++ faces matched the same build123d face")
    return {fid: faces[j] for fid, j in matched.items()}


def feature_of_face(feats):
    owner = {}
    for f in feats:
        for fid in f.face_ids:
            owner[fid] = f
    return owner


def surface_view(part):
    g = te.analyze(to_brep_string(part))
    faces = match_faces(part, g)
    view = []
    for n in g.faces:
        color = SURFACE_COLORS.get(n.surface_type, "steelblue")
        view.append((faces[n.id], f"{n.id} {n.surface_type}", color))
    return view


def feature_view(part):
    brep = to_brep_string(part)
    g = te.analyze(brep)
    faces = match_faces(part, g)
    owner = feature_of_face(te.recognize(brep))

    view = []
    for n in g.faces:
        f = owner.get(n.id)
        if f:
            view.append((faces[n.id], f"{n.id} {f.type.name}", FEATURE_COLORS[f.type]))
        else:
            view.append((faces[n.id], f"{n.id}", PLAIN))
    return view


def defeature_view(part, *kinds):
    """Before on the left, after on the right. Orange = the face really changed."""
    brep = to_brep_string(part)
    before = te.analyze(brep)

    doomed = set()
    for f in te.recognize(brep):
        if not kinds or f.type in kinds:
            doomed.update(f.face_ids)

    ok, out, face_map = te.defeature(brep, sorted(doomed))
    if not ok:
        raise RuntimeError("OCCT refused to defeature this one")

    after = te.analyze(out)
    after_faces = match_faces(from_brep_string(out), after)
    shift = Location((part.bounding_box().size.X * 1.5, 0, 0))

    view = feature_view(part)
    for old, track in face_map.items():
        for new in track.new_ids:
            # OCCT says everything is Modified, so check for myself
            grew = abs(after.faces[new - 1].area - before.faces[old - 1].area) > 1e-6
            color = CHANGED if grew else PLAIN
            view.append((after_faces[new].moved(shift), f"{old} -> {new}", color))
    return view


def show_view(view):
    faces = [face for face, _, _ in view]
    names = [name for _, name, _ in view]
    colors = [color for _, _, color in view]
    show(*faces, names=names, colors=colors, reset_camera=Camera.RESET)


def to_nx(g, feats=()):
    owner = feature_of_face(feats)
    G = nx.Graph()
    for f in g.faces:
        feat = owner.get(f.id)
        G.add_node(
            f.id,
            type=f.surface_type,
            area=f.area,
            feature=feat.type.name if feat else "",
        )
    for a in g.arcs:
        G.add_edge(a.face_a, a.face_b, kind=a.kind.name, angle=a.angle_deg)
    return G


def write_graphml(part, path):
    brep = to_brep_string(part)
    nx.write_graphml(to_nx(te.analyze(brep), te.recognize(brep)), path)


if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "filleted_bracket"
    mode = sys.argv[2] if len(sys.argv) > 2 else "features"
    part = getattr(parts, name)()

    if mode == "surface":
        show_view(surface_view(part))
    elif mode == "features":
        show_view(feature_view(part))
    elif mode == "defeature":
        show_view(defeature_view(part))
    else:
        sys.exit(f"don't know '{mode}', try surface, features or defeature")
