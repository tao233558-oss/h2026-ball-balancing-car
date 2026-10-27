"""Selection for the archived YOLO11 (boxes, class_ids, scores) result layout."""
import math


def target_center(target):
    x, y, width, height = target[:4]
    return x + width // 2, y + height // 2


def select_best_target(result, width=800, height=480, threshold=0.5):
    if result is None or len(result) < 3:
        return None
    boxes, scores = result[0], result[2]
    if boxes is None or scores is None:
        return None
    best = None
    for index in range(min(len(boxes), len(scores))):
        box = boxes[index]
        if box is None or len(box) != 4:
            continue
        try:
            score = float(scores[index])
            raw = [float(value) for value in box]
        except (TypeError, ValueError, OverflowError):
            continue
        if not math.isfinite(score) or not all(math.isfinite(v) for v in raw):
            continue
        if not (threshold <= score <= 1.0) or raw[2] <= 0 or raw[3] <= 0:
            continue
        x, y, w, h = [int(round(value)) for value in raw]
        if w <= 0 or h <= 0:
            continue
        candidate = (x, y, w, h, score, len(boxes))
        cx, cy = target_center(candidate)
        if not (0 <= cx < width and 0 <= cy < height):
            continue
        if best is None or score > best[4]:
            best = candidate
    return best
