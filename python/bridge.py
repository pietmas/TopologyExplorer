import os
import tempfile
from build123d import export_brep


def to_brep_string(shape) -> str:
    with tempfile.NamedTemporaryFile(suffix=".brep", delete=False) as f:
        path = f.name
    try:
        export_brep(shape, path)
        with open(path) as f:
            return f.read()
    finally:
        os.unlink(path)
