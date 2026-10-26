# Steel-ball YOLO dataset

- Class: `steel_ball` (class id `0`)
- Train: 32 reviewed images / 120 boxes
- Val: 8 reviewed images / 20 boxes
- Review pending (excluded): 12 images / 616 boxes
- Split seed: `230`

Only source rows marked `checked` are used for training and validation.
The `review_pending` directory is deliberately excluded until those dense or
blurred annotations have been manually reviewed.

`calibration/` contains representative training images for nncase PTQ.
`tools/train_yolov8.py` creates a runtime YAML with an absolute dataset path,
so this repository can be moved or cloned without editing the checked-in YAML.
