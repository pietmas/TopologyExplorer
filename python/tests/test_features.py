import pytest
import topoexplorer as te
from bridge import to_brep_string
from parts import (block, block_with_blind_hole, block_with_boss,
                   filleted_block, filleted_bracket, l_block, plate_with_holes,
                   pocketed_block, split_hole_plate)


def test_block_counts():
    g = te.analyze(to_brep_string(block()))
    assert (g.face_count, g.edge_count, g.vertex_count) == (6, 12, 8)
    assert len(g.arcs) == 12


def test_filleted_block_has_cylinders():
    g = te.analyze(to_brep_string(filleted_block()))
    types = [f.surface_type for f in g.faces]
    assert types.count("Cylinder") == 12
    assert "Sphere" in types


def test_block_euler():
    assert te.analyze(to_brep_string(block())).euler == 2


@pytest.mark.parametrize("n", [1, 3, 4])
def test_plate_with_holes_count(n):
    g = te.analyze(to_brep_string(plate_with_holes(n=n)))
    types = [f.surface_type for f in g.faces]
    assert types.count("Cylinder") == n


def test_plate_with_holes_euler():
    g = te.analyze(to_brep_string(plate_with_holes(n=4)))
    assert g.euler == -6


def _kinds(part):
    g = te.analyze(to_brep_string(part))
    types = {f.id: f.surface_type for f in g.faces}
    return [(a.kind, {types[a.face_a], types[a.face_b]}) for a in g.arcs], g


def test_block_all_convex():
    g = te.analyze(to_brep_string(block()))
    assert all(a.kind == te.EdgeKind.Convex for a in g.arcs)
    assert all(abs(a.angle_deg - 90) < 1e-3 for a in g.arcs)


def test_through_hole_rims_are_convex():
    arcs, _ = _kinds(plate_with_holes(4))
    rims = [k for k, t in arcs if t == {"Plane", "Cylinder"}]
    assert len(rims) == 8
    assert all(k == te.EdgeKind.Convex for k in rims)
    assert not any(k == te.EdgeKind.Concave for k, _ in arcs)


def test_l_block_has_one_concave_edge():
    arcs, _ = _kinds(l_block())
    assert [k for k, _ in arcs].count(te.EdgeKind.Concave) == 1


def test_blind_hole_floor_concave_rim_convex():
    arcs, _ = _kinds(block_with_blind_hole())
    rim_and_floor = sorted(k.name for k, t in arcs if t == {"Plane", "Cylinder"})
    assert rim_and_floor == ["Concave", "Convex"]


def test_filleted_block_all_smooth():
    arcs, _ = _kinds(filleted_block())
    assert len(arcs) == 48
    assert all(k == te.EdgeKind.Smooth for k, _ in arcs)


def _features(part):
    brep = to_brep_string(part)
    types = {f.id: f.surface_type for f in te.analyze(brep).faces}
    return te.recognize(brep), types


def _of(feats, kind):
    return [f for f in feats if f.type == kind]


def test_recognizes_four_holes():
    feats, _ = _features(plate_with_holes(4))
    holes = _of(feats, te.FeatureType.Hole)
    assert len(holes) == 4
    assert all(abs(f.radius - 4) < 1e-6 for f in holes)
    assert len(feats) == 4


def test_blind_hole_includes_floor():
    feats, types = _features(block_with_blind_hole())
    assert len(feats) == 1
    (hole,) = feats
    assert hole.type == te.FeatureType.Hole
    assert abs(hole.radius - 5) < 1e-6
    assert sorted(types[i] for i in hole.face_ids) == ["Cylinder", "Plane"]


def test_split_cylinder_is_one_hole():
    feats, types = _features(split_hole_plate())
    assert len(feats) == 1
    (hole,) = feats
    assert hole.type == te.FeatureType.Hole
    assert [types[i] for i in hole.face_ids] == ["Cylinder", "Cylinder"]


def test_filleted_block_features():
    feats, types = _features(filleted_block())
    fillets = _of(feats, te.FeatureType.Fillet)
    assert len(feats) == len(fillets) == 20
    assert all(f.convex and abs(f.radius - 3) < 1e-6 for f in fillets)
    kinds = [types[f.face_ids[0]] for f in fillets]
    assert kinds.count("Cylinder") == 12 and kinds.count("Sphere") == 8


def test_bracket_inner_and_outer_fillets():
    feats, _ = _features(filleted_bracket(inner_r=3, outer_r=5))
    fillets = sorted(_of(feats, te.FeatureType.Fillet), key=lambda f: f.radius)
    assert len(feats) == len(fillets) == 2
    assert not fillets[0].convex and abs(fillets[0].radius - 3) < 1e-6
    assert fillets[1].convex and abs(fillets[1].radius - 5) < 1e-6


def test_pocket():
    feats, _ = _features(pocketed_block())
    assert [f.type for f in feats] == [te.FeatureType.Pocket]
    assert len(feats[0].face_ids) == 5


@pytest.mark.parametrize("round_boss,faces", [(False, 5), (True, 2)])
def test_boss(round_boss, faces):
    feats, _ = _features(block_with_boss(round_boss))
    assert [f.type for f in feats] == [te.FeatureType.Boss]
    assert len(feats[0].face_ids) == faces


@pytest.mark.parametrize("part", [block, l_block])
def test_no_features(part):
    feats, _ = _features(part())
    assert feats == []
