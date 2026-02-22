# Traffic Guard 

A DeepStream 7.0 pipeline in C that processes h264 video, detects, tracks and classifes vehicles, detects and reads license plates (LPR), and forwards events to an MQTT broker.

Built and tested on Ubuntu 22.04 with:

```
NVIDIA GeForce RTX 4050 - Driver 535.288.01 - CUDA 12.2 - DeepStream 7.0
```

Running inside a dev container is recommended. Run the command `xhost +local:docker` so the container can render the GStreamer/DeepStream video output window directly on your host display.


## Prerequisites

### 1. Download the models

Download the inference models and place them in the `models/` directory. The pipeline uses the following models:

| Config file | Model |
|---|---|
| `pgie_trafficcamnet_config.yml` | [TrafficCamNet](https://catalog.ngc.nvidia.com/orgs/nvidia/teams/tao/models/trafficcamnet) |
| `sgie_vehiclemakenet_config.yml` | [VehicleMakeNet](https://catalog.ngc.nvidia.com/orgs/nvidia/teams/tao/models/vehiclemakenet) |
| `sgie_vehicletypenet_config.yml` | [VehicleTypeNet](https://catalog.ngc.nvidia.com/orgs/nvidia/teams/tao/models/vehicletypenet) |
| `sgie_lpd_us_config.yml` | [LPDNet - License Plate Detector](https://catalog.ngc.nvidia.com/orgs/nvidia/teams/tao/models/lpdnet) |
| `sgie_lpr_us_config.yml` | [LPRNet - License Plate Reader](https://catalog.ngc.nvidia.com/orgs/nvidia/teams/tao/models/lprnet) |

### 2. Download the input video

The pipeline has been tested with [Car Number Plate Video](https://www.kaggle.com/datasets/nimishshandilya/car-number-plate-video?resource=download).

If your video is in `.mp4`, convert it to `h264` using the convenience script:

```sh
bash scripts/convert_h264.sh <input.mp4>
```
Requires `ffmpeg`.

### 3. Update the configuration

Review and adjust `configs/deepstream_config.yml` before running:

- `source.location`: path to the input `h264` video file
- `msgbroker.conn-str`: MQTT broker address and port (default `127.0.0.1;1883`)
- `msgbroker.topic`: MQTT topic
- `tracker.ll-config-file`: tracker algorithm config
- GIE `config-file-path` entries must point to the downloaded model configs

## Build

**1. Build the custom LPR parser shared library:**

```sh
make custom_parser
```

**2. Build the application binary:**

```sh
make
```

Binary is produced at `bin/`.


## Run

```sh
DEEPSTREAM_CONFIG_YAML=./configs/deepstream_config.yml ./bin/traffic-guard
```

An `MQTT` broker is needed for the app to run. See section [pipeline architecture](#pipeline-architecture). Run the compose file in `infra/` to spawn a MQTT broker.

## Pipeline architecture

```
filesrc
  └─► h264parse
        └─► nvv4l2decoder
              └─► nvstreammux
                    └─► nvinfer (TrafficCamNet)
                          └─► nvtracker
                                └─► nvinfer (VehicleMakeNet)
                                      └─► nvinfer (VehicleTypeNet)
                                            └─► nvinfer (LPDNet)
                                                  └─► nvinfer (LPRNet)
                                                        └─► nvvideoconvert
                                                              └─► nvdsosd
                                                                    └─► tee
                                                                         ├─► queue1
                                                                         │     └─► nvmsgconv
                                                                         │           └─► nvmsgbroker (MQTT)
                                                                         └─► queue2
                                                                               └─► nveglglessink / nv3dsink
```
