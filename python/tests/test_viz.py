import networkx as nx
import pytest
import topoexplorer as te
from bridge import from_brep_string, to_brep_string
from parts import (block_with_blind_hole, block_with_boss, filleted_block,
                   filleted_bracket, plate_with_holes, pocketed_block,
                   split_hole_plate)
from viz import (CHANGED, FEATURE_COLORS, PLAIN, defeature_view, feature_view,
                 match_faces, to_nx)

ZOO = [plate_with_holes, block_with_blind_hole, filleted_block, filleted_bracket,
       pocketed_block, block_with_boss, split_hole_plate]


@pytest.mark.parametrize("part", ZOO)
def test_every_face_finds_its_twin(part):
    p = part()
    g = te.analyze(to_brep_string(p))
    faces = match_faces(p, g)
    assert sorted(faces) == [n.id for n in g.faces]
    for n in g.faces:
        assert abs(faces[n.id].area - n.area) < 1e-6


def test_matching_survives_a_round_trip():
    brep = to_brep_string(filleted_block())
    back = from_brep_string(brep)
    assert len(match_faces(back, te.analyze(brep))) == 26


def test_holes_are_painted_red():
    view = feature_view(plate_with_holes(4))
    colors = [c for _, _, c in view]
    assert colors.count(FEATURE_COLORS[te.FeatureType.Hole]) == 4
    assert colors.count(PLAIN) == 6


def test_defeature_view_shows_who_grew():
    view = defeature_view(plate_with_holes(4), te.FeatureType.Hole)
    after = [color for _, name, color in view if "->" in name]
    # top and bottom got their holes filled, the 4 sides didn't move
    assert sorted(after) == sorted([CHANGED] * 2 + [PLAIN] * 4)


def test_defeature_view_refuses_split_hole():
    with pytest.raises(RuntimeError):
        defeature_view(split_hole_plate(), te.FeatureType.Hole)


def test_graph_export(tmp_path):
    brep = to_brep_string(filleted_bracket())
    g = te.analyze(brep)
    G = to_nx(g, te.recognize(brep))
    assert G.number_of_nodes() == g.face_count
    assert G.number_of_edges() == len(g.arcs)
    feats = [d["feature"] for _, d in G.nodes(data=True) if d["feature"]]
    assert feats == ["Fillet", "Fillet"]
    nx.write_graphml(G, tmp_path / "bracket.graphml")
    assert nx.read_graphml(tmp_path / "bracket.graphml").number_of_nodes() == 10
