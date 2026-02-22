#ifndef PIPELINE_CONTROLLER_H
#define PIPELINE_CONTROLLER_H

#include <gst/gst.h>

/** Owns the pipeline and main loop; the bus watch quits the loop on EOS or error. */
typedef struct PipelineController PipelineController;

PipelineController *pipeline_controller_new(GstElement *pipeline);
void pipeline_controller_free(PipelineController *controller);
void pipeline_controller_play(PipelineController *controller);
void pipeline_controller_pause(PipelineController *controller);
void pipeline_controller_stop(PipelineController *controller);
/** Blocks until the bus watch quits (EOS or error). */
void pipeline_controller_run_loop(PipelineController *controller);

#endif
