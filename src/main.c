#include <gst/gst.h>
#include <stdlib.h>

#include "config.h"
#include "director.h"
#include "logger.h"
#include "pipeline_controller.h"

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    const char *yaml_path = config_get_yaml_path();
    if (!yaml_path || yaml_path[0] == '\0') {
        log_error("Set DEEPSTREAM_CONFIG_YAML to the path to "
                  "configs/deepstream_config.yml");
        return EXIT_FAILURE;
    }

    GstElement *pipeline = director_build(yaml_path);
    if (!pipeline) {
        log_error("main: failed to build pipeline from %s", yaml_path);
        return EXIT_FAILURE;
    }

    PipelineController *controller = pipeline_controller_new(pipeline);
    if (!controller) {
        log_error("main: failed to create pipeline controller");
        gst_object_unref(pipeline);
        return EXIT_FAILURE;
    }

    pipeline_controller_play(controller);
    pipeline_controller_run_loop(controller);
    pipeline_controller_stop(controller);
    pipeline_controller_free(controller);
    gst_object_unref(pipeline);

    return EXIT_SUCCESS;
}
