# Metrics

Compute object detection and LPR metrics by comparing DeepStream per-frame detections to COCO ground truth from CVAT.

Requires [uv](https://docs.astral.sh/uv/) 0.5.24+.

## Setup

```bash
uv sync
```

## Run

```bash
uv run compute_detection_metrics.py
```

with custom paths:

```bash
uv run compute_detection_metrics.py \
  --coco ../../data/annotations/instances_default.json \
  --detections-dir ../../logs/detections \
  --output metrics_summary.json
```


## How to generate detections from the pipeline

The probe `probe_write_detections` is implemented in `app/src/probes/probe_detections.c` but is not attached to the pipeline by default. To enable it, add one line in `app/src/director.c` after the existing probe registrations:

```c
probe_base_add_buffer_probe(nvosd, "sink", probe_write_detections, NULL);
```

Detections are written to `logs/detections/` by default. Override the output directory with the `DETECTION_OUTPUT_DIR` environment variable if needed.

```bash
DETECTION_OUTPUT_DIR=/path/to/output ./bin/traffic-guard
```

The output directory is created automatically if it does not exist.