
"""
Compute object detection and LPR metrics by comparing DeepStream per-frame
detections to COCO ground truth from CVAT.

Each prediction file covers one frame at 1920x1080. Lines take the form:
  car left top width height plate_text
  plate left top width height plate_text

Usage:
  python compute_detection_metrics.py
  python compute_detection_metrics.py --coco path/to/instances.json --detections-dir path/to/detections
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


def _safe_div(num: float, den: float) -> float:
    return num / den if den > 0 else 0.0


@dataclass
class DetectionCounts:
    tp: int = 0
    fp: int = 0
    fn: int = 0

    def update(self, pairs: list, unmatched_pred: list, unmatched_gt: list) -> None:
        self.tp += len(pairs)
        self.fp += len(unmatched_pred)
        self.fn += len(unmatched_gt)

    def add(self, other: "DetectionCounts") -> None:
        self.tp += other.tp
        self.fp += other.fp
        self.fn += other.fn

    @property
    def precision(self) -> float:
        return _safe_div(self.tp, self.tp + self.fp)

    @property
    def recall(self) -> float:
        return _safe_div(self.tp, self.tp + self.fn)

    @property
    def f1(self) -> float:
        return _safe_div(2 * self.precision * self.recall, self.precision + self.recall)


@dataclass
class LprCounts:
    exact: int = 0
    total: int = 0
    cer_sum: float = 0.0

    def add(self, other: "LprCounts") -> None:
        self.exact += other.exact
        self.total += other.total
        self.cer_sum += other.cer_sum

    @property
    def exact_rate(self) -> float:
        return _safe_div(self.exact, self.total)

    @property
    def mean_cer(self) -> float:
        return _safe_div(self.cer_sum, self.total)


def bbox_xywh_to_xyxy(
    box: tuple[float, float, float, float],
) -> tuple[float, float, float, float]:
    x, y, w, h = box
    return (x, y, x + w, y + h)


def bbox_iou(
    box_a: tuple[float, float, float, float],
    box_b: tuple[float, float, float, float],
    format_xywh: bool = True,
) -> float:
    if format_xywh:
        box_a = bbox_xywh_to_xyxy(box_a)
        box_b = bbox_xywh_to_xyxy(box_b)
    ax1, ay1, ax2, ay2 = box_a
    bx1, by1, bx2, by2 = box_b
    inter_x1, inter_y1 = max(ax1, bx1), max(ay1, by1)
    inter_x2, inter_y2 = min(ax2, bx2), min(ay2, by2)
    if inter_x2 <= inter_x1 or inter_y2 <= inter_y1:
        return 0.0
    inter = (inter_x2 - inter_x1) * (inter_y2 - inter_y1)
    union = (ax2 - ax1) * (ay2 - ay1) + (bx2 - bx1) * (by2 - by1) - inter
    return _safe_div(inter, union)


def scale_bbox(
    left: float, top: float, width: float, height: float,
    scale_x: float, scale_y: float,
) -> tuple[float, float, float, float]:
    return (left * scale_x, top * scale_y, width * scale_x, height * scale_y)


def normalize_plate_text(s: str) -> str:
    """Uppercase and strip; a bare '-' (no-plate sentinel) maps to empty string."""
    if not s or s == "-":
        return ""
    return " ".join(s.upper().strip().split())


def levenshtein_distance(a: str, b: str) -> int:
    if len(a) < len(b):
        a, b = b, a
    n, m = len(a), len(b)
    prev = list(range(m + 1))
    for i in range(1, n + 1):
        curr = [i]
        for j in range(1, m + 1):
            cost = 0 if a[i - 1] == b[j - 1] else 1
            curr.append(min(prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost))
        prev = curr
    return prev[m]


def character_error_rate(pred: str, gt: str) -> float:
    """CER = edit_distance / len(gt). An empty gt means nothing to recognise, so returns 0.0."""
    pred = normalize_plate_text(pred)
    gt = normalize_plate_text(gt)
    if not gt:
        return 0.0 if not pred else 1.0
    return levenshtein_distance(pred, gt) / len(gt)


def parse_detection_line(line: str) -> tuple | None:
    """Parse a single detection line into (kind, left, top, width, height, plate_text).

    Expected format: 'car|plate left top width height [text]'
    Returns None for empty or malformed lines.
    """
    line = line.strip()
    if not line:
        return None
    parts = line.split()
    if len(parts) < 5 or parts[0].lower() not in ("car", "plate"):
        return None
    kind = parts[0].lower()
    try:
        left, top, width, height = float(parts[1]), float(parts[2]), float(parts[3]), float(parts[4])
    except ValueError:
        return None
    plate_text = " ".join(parts[5:]) if len(parts) > 5 else "-"
    return (kind, left, top, width, height, plate_text)


def load_predictions_for_frame(detections_dir: Path, frame_num: int) -> tuple[list, list]:
    """Read car and plate detections from frame_{frame_num:06d}.txt.

    Each returned list contains (left, top, width, height, plate_text) tuples.
    Missing files are treated as frames with no detections.
    """
    path = detections_dir / f"frame_{frame_num:06d}.txt"
    cars, plates = [], []
    if not path.exists():
        return cars, plates
    with open(path) as f:
        for line in f:
            parsed = parse_detection_line(line)
            if not parsed:
                continue
            kind, left, top, width, height, plate_text = parsed
            (plates if kind == "plate" else cars).append((left, top, width, height, plate_text))
    return cars, plates


def coco_frame_index_from_file_name(file_name: str) -> int | None:
    """Parse the frame index from a filename like 'frame_000042.png' -> 42.

    Returns None when the filename does not match the expected pattern.
    """
    m = re.match(r"frame_(\d+)\.\w+", file_name)
    return int(m.group(1)) if m else None


def load_coco(coco_path: Path) -> tuple[list, list, dict]:
    with open(coco_path) as f:
        data = json.load(f)
    categories = data.get("categories", [])
    id_to_cat = {c["id"]: c["name"] for c in categories}
    return data.get("images", []), data.get("annotations", []), id_to_cat


def build_gt_by_image(images: list, annotations: list, id_to_cat: dict) -> tuple[dict, dict, dict]:
    """Organise GT annotations into per-image lookup tables.

    Returns three dicts keyed by image_id:
      cars_by_iid:   image_id -> [(x, y, w, h), ...]
      plates_by_iid: image_id -> [(x, y, w, h, text), ...]
      size_by_iid:   image_id -> (width, height)
    """
    cars_by_iid: dict[int, list] = {}
    plates_by_iid: dict[int, list] = {}
    size_by_iid: dict[int, tuple] = {}
    for img in images:
        iid = img["id"]
        size_by_iid[iid] = (img["width"], img["height"])
        cars_by_iid[iid] = []
        plates_by_iid[iid] = []
    for ann in annotations:
        iid = ann["image_id"]
        cat = id_to_cat.get(ann["category_id"], "")
        bbox = tuple(ann["bbox"])
        if cat == "car":
            cars_by_iid[iid].append(bbox)
        elif cat == "license_plate":
            text = (ann.get("attributes") or {}).get("value", "")
            plates_by_iid[iid].append((*bbox, text))
    return cars_by_iid, plates_by_iid, size_by_iid


def _plate_center_inside_car(plate_xywh: tuple, car_xywh: tuple) -> bool:
    px, py, pw, ph = plate_xywh
    cx, cy, cw, ch = car_xywh
    return cx <= px + pw / 2 <= cx + cw and cy <= py + ph / 2 <= cy + ch


def associate_plates_to_cars(plates: list, cars: list) -> list:
    """For each car, find the first plate whose centre falls within the car bbox.

    Greedy: plates are claimed in iteration order and each is assigned to at most one car.
    plates: [(x, y, w, h, text), ...], cars: [(x, y, w, h), ...]
    Returns a list of len(cars) containing plate text or None.
    """
    result = [None] * len(cars)
    used = [False] * len(plates)
    for ci, car in enumerate(cars):
        for pi, pl in enumerate(plates):
            if used[pi]:
                continue
            if _plate_center_inside_car(pl[:4], car):
                result[ci] = pl[4]
                used[pi] = True
                break
    return result


def match_pred_to_gt(
    pred_boxes: list,
    gt_boxes: list,
    iou_threshold: float,
) -> tuple[list, list, list]:
    """Greedy 1-to-1 matching: pairs predictions and GT boxes by descending IoU.

    Only pairs with IoU >= iou_threshold are considered.
    Returns (matched_pairs, unmatched_pred_indices, unmatched_gt_indices)
    where each matched pair is (pred_idx, gt_idx).
    """
    if not pred_boxes or not gt_boxes:
        return [], list(range(len(pred_boxes))), list(range(len(gt_boxes)))
    candidates = []
    for pi, pb in enumerate(pred_boxes):
        for gi, gb in enumerate(gt_boxes):
            iou = bbox_iou(pb, gb)
            if iou >= iou_threshold:
                candidates.append((iou, pi, gi))
    candidates.sort(key=lambda x: -x[0])
    matched_p, matched_g, pairs = set(), set(), []
    for _, pi, gi in candidates:
        if pi not in matched_p and gi not in matched_g:
            matched_p.add(pi)
            matched_g.add(gi)
            pairs.append((pi, gi))
    return (
        pairs,
        [i for i in range(len(pred_boxes)) if i not in matched_p],
        [i for i in range(len(gt_boxes)) if i not in matched_g],
    )


def _evaluate_frame(
    image: dict,
    cars_by_iid: dict,
    plates_by_iid: dict,
    size_by_iid: dict,
    detections_dir: Path,
    frame_offset: int,
    det_width: int,
    det_height: int,
    iou_threshold: float,
) -> tuple[DetectionCounts, DetectionCounts, LprCounts] | None:
    """Run car, plate, and LPR evaluation for one COCO image.

    Returns None when the image filename does not match the expected frame pattern.
    """
    image_id = image["id"]
    coco_frame_idx = coco_frame_index_from_file_name(image.get("file_name", ""))
    if coco_frame_idx is None:
        return None

    gt_w, gt_h = size_by_iid.get(image_id, (3840, 2160))
    scale_x, scale_y = gt_w / det_width, gt_h / det_height

    pred_cars, pred_plates = load_predictions_for_frame(detections_dir, coco_frame_idx + frame_offset)
    gt_cars = cars_by_iid.get(image_id, [])
    gt_plates = plates_by_iid.get(image_id, [])

    pred_car_boxes = [scale_bbox(left, top, w, h, scale_x, scale_y) for left, top, w, h, _ in pred_cars]
    pred_plate_boxes = [scale_bbox(left, top, w, h, scale_x, scale_y) for left, top, w, h, _ in pred_plates]
    gt_plate_boxes = [pl[:4] for pl in gt_plates]

    car_counts = DetectionCounts()
    lpd_counts = DetectionCounts()
    lpr_counts = LprCounts()

    car_pairs, unmatched_cp, unmatched_cg = match_pred_to_gt(pred_car_boxes, gt_cars, iou_threshold)
    car_counts.update(car_pairs, unmatched_cp, unmatched_cg)

    lpd_pairs, unmatched_pp, unmatched_pg = match_pred_to_gt(pred_plate_boxes, gt_plate_boxes, iou_threshold)
    lpd_counts.update(lpd_pairs, unmatched_pp, unmatched_pg)

    gt_car_to_plate = associate_plates_to_cars(gt_plates, gt_cars)
    pred_car_texts = [text for _, _, _, _, text in pred_cars]
    for pred_idx, gt_idx in car_pairs:
        gt_text = gt_car_to_plate[gt_idx] if gt_idx < len(gt_car_to_plate) else None
        if gt_text is None:
            continue
        pred_text = pred_car_texts[pred_idx] if pred_idx < len(pred_car_texts) else "-"
        lpr_counts.total += 1
        if normalize_plate_text(pred_text) == normalize_plate_text(gt_text):
            lpr_counts.exact += 1
        lpr_counts.cer_sum += character_error_rate(pred_text, gt_text)

    return car_counts, lpd_counts, lpr_counts


def evaluate(
    images: list,
    cars_by_iid: dict,
    plates_by_iid: dict,
    size_by_iid: dict,
    detections_dir: Path,
    frame_offset: int,
    det_width: int,
    det_height: int,
    iou_threshold: float,
) -> tuple[DetectionCounts, DetectionCounts, LprCounts]:
    car_total = DetectionCounts()
    lpd_total = DetectionCounts()
    lpr_total = LprCounts()
    for image in images:
        result = _evaluate_frame(
            image, cars_by_iid, plates_by_iid, size_by_iid,
            detections_dir, frame_offset, det_width, det_height, iou_threshold,
        )
        if result is None:
            continue
        car_frame, lpd_frame, lpr_frame = result
        car_total.add(car_frame)
        lpd_total.add(lpd_frame)
        lpr_total.add(lpr_frame)
    return car_total, lpd_total, lpr_total


def build_summary(car: DetectionCounts, lpd: DetectionCounts, lpr: LprCounts) -> dict:
    def det_dict(c: DetectionCounts) -> dict:
        return {
            "mAP@0.5": round(c.precision, 4),  # with a single confidence threshold, AP@0.5 reduces to precision
            "precision": round(c.precision, 4),
            "recall": round(c.recall, 4),
            "f1": round(c.f1, 4),
            "TP": c.tp,
            "FP": c.fp,
            "FN": c.fn,
        }
    return {
        "car_detection": det_dict(car),
        "license_plate_detection": det_dict(lpd),
        "lpr": {
            "exact_match_rate": round(lpr.exact_rate, 4),
            "character_error_rate_mean": round(lpr.mean_cer, 4),
            "pairs_evaluated": lpr.total,
        },
    }


def print_summary(summary: dict) -> None:
    def _print_det(title: str, key: str) -> None:
        d = summary[key]
        print(f"## {title}")
        print(f"  mAP@0.5:   {d['mAP@0.5']}")
        print(f"  Precision: {d['precision']}")
        print(f"  Recall:    {d['recall']}")
        print(f"  F1:        {d['f1']}")
        print(f"  TP/FP/FN:  {d['TP']} / {d['FP']} / {d['FN']}")
        print()

    _print_det("Car detection (vs COCO car)", "car_detection")
    _print_det("License plate detection (vs COCO license_plate)", "license_plate_detection")
    lpr = summary["lpr"]
    print("## LPR (plate text)")
    print(f"  Exact match rate:  {lpr['exact_match_rate']}")
    print(f"  CER (mean):        {lpr['character_error_rate_mean']}")
    print(f"  Pairs evaluated:   {lpr['pairs_evaluated']}")


def build_arg_parser() -> argparse.ArgumentParser:
    repo_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(
        description="Compute detection and LPR metrics vs COCO ground truth.",
    )
    parser.add_argument(
        "--coco",
        type=Path,
        default=repo_root / "data" / "annotations" / "instances_default.json",
        help="Path to COCO instances JSON (default: data/annotations/instances_default.json)",
    )
    parser.add_argument(
        "--detections-dir",
        type=Path,
        default=repo_root / "logs" / "detections",
        help="Directory with frame_NNNNNN.txt prediction files",
    )
    parser.add_argument(
        "--frame-offset",
        type=int,
        default=1,
        help="Detection frame number = COCO frame index + this (default 1: COCO frame_000000 -> frame_000001.txt)",
    )
    parser.add_argument(
        "--detection-width",
        type=int,
        default=1920,
        help="Resolution width of predictions (default 1920)",
    )
    parser.add_argument(
        "--detection-height",
        type=int,
        default=1080,
        help="Resolution height of predictions (default 1080)",
    )
    parser.add_argument(
        "--iou-threshold",
        type=float,
        default=0.5,
        help="IoU threshold for matching (default 0.5)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Optional path to write JSON summary",
    )
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()

    if not args.coco.exists():
        print(f"COCO file not found: {args.coco}", file=sys.stderr)
        return 1
    if not args.detections_dir.exists():
        print(f"Detections dir not found: {args.detections_dir}", file=sys.stderr)
        return 1

    images, annotations, id_to_cat = load_coco(args.coco)
    cars_by_iid, plates_by_iid, size_by_iid = build_gt_by_image(images, annotations, id_to_cat)

    car_counts, lpd_counts, lpr_counts = evaluate(
        images, cars_by_iid, plates_by_iid, size_by_iid,
        args.detections_dir, args.frame_offset,
        args.detection_width, args.detection_height, args.iou_threshold,
    )

    summary = build_summary(car_counts, lpd_counts, lpr_counts)
    print_summary(summary)

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, "w") as f:
            json.dump(summary, f, indent=2)
        print(f"\nWrote summary to {args.output}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
