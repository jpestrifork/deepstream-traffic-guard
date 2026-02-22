#include <stdlib.h>
#include "config.h"

#define DEFAULT_YAML_PATH        "configs/deepstream_config.yml"
#define DEFAULT_DETECTION_OUTPUT "logs/detections"

const char *config_get_yaml_path(void)
{
    const char *path = getenv("DEEPSTREAM_CONFIG_YAML");
    if (!path)
        path = getenv("DEEPSTREAM_CONFIG");
    if (!path)
        path = DEFAULT_YAML_PATH;
    return path;
}

const char *config_get_detection_output_dir(void)
{
    const char *dir = getenv("DETECTION_OUTPUT_DIR");
    if (!dir)
        dir = DEFAULT_DETECTION_OUTPUT;
    return dir;
}
