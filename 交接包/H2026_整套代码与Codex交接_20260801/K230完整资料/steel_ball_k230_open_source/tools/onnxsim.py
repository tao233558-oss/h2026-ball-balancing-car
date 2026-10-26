"""Compatibility layer for the official K230 YOLOv8 converter.

The official converter imports onnxsim 0.4.36, but that release has no
prebuilt Python 3.12 wheel for Windows. The exported steel-ball ONNX already
has static shapes and passes ONNX validation, so shape inference is sufficient
for nncase. The converter performs shape inference before calling simplify().
"""

from types import SimpleNamespace

import numpy as np
import onnx


# onnx 1.22 removed the public onnx.mapping alias used by the official script.
if not hasattr(onnx, "mapping"):
    onnx.mapping = SimpleNamespace(
        TENSOR_TYPE_TO_NP_TYPE={
            onnx.TensorProto.FLOAT: np.dtype("float32"),
        }
    )


def simplify(model, input_shapes=None):
    """Return the validated, shape-inferred model without graph rewriting."""
    del input_shapes
    onnx.checker.check_model(model)
    return model, True
