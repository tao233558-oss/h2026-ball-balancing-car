# Third-party and data notices

## Ultralytics YOLO

Training and export use the `ultralytics` Python package. Ultralytics states
that its open-source code and trained/fine-tuned models are licensed under
GNU AGPL-3.0 unless an Enterprise License applies:

- https://www.ultralytics.com/license
- https://www.ultralytics.com/legal/agpl-3-0-software-license

## nncase and CanMV

KModel conversion uses Canaan nncase/nncase-kpu 2.11.0 and the official
K230 YOLOv8 conversion example. nncase package metadata identifies the
project license as Apache License 2.0:

- https://github.com/kendryte/nncase
- https://github.com/kendryte/canmv_k230
- https://www.kendryte.com/k230_canmv/en/main/ai_dev_doc.html

Third-party source archives, binaries and installers are intentionally not
vendored in this repository. Setup scripts download them from their official
release locations.

## Dataset images

The repository structure contains the images supplied for this steel-ball
project and their annotations. No independent image provenance or
redistribution permission could be verified during project preparation.
Before publishing the repository, the publisher must confirm that every image
may legally be redistributed and document the chosen dataset license.

