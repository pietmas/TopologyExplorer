import pytest
import topoexplorer as te
from bridge import to_brep_string
from parts import (block, block_with_blind_hole, filleted_block, l_block,
                   plate_with_holes)


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
