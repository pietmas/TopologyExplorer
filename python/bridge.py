import os
import tempfile
from build123d import export_brep, import_brep


def to_brep_string(shape) -> str:
    with tempfile.NamedTemporaryFile(suffix=".brep", delete=False) as f:
        path = f.name
    try:
        export_brep(shape, path)
        with open(path) as f:
            return f.read()
    finally:
        os.unlink(path)


def from_brep_string(brep: str):
    with tempfile.NamedTemporaryFile("w", suffix=".brep", delete=False) as f:
        f.write(brep)
        path = f.name
    try:
        return import_brep(path)
    finally:
        os.unlink(path)
