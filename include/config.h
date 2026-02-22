#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/** Path from DEEPSTREAM_CONFIG_YAML, or DEEPSTREAM_CONFIG; default configs/deepstream_config.yml */
const char *config_get_yaml_path(void);

/** Directory from DETECTION_OUTPUT_DIR; default logs/detections */
const char *config_get_detection_output_dir(void);

#endif
