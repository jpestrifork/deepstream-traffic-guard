#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <gst/gst.h>

/**
 * Opaque handle for element creation; linking is handled by PipelineLinker.
 * Pointers returned by add_* belong to the pipeline bin; do not unref them.
 */
typedef struct PipelineBuilder PipelineBuilder;

PipelineBuilder *pipeline_builder_new(const char *config_path);
void             pipeline_builder_free(PipelineBuilder *builder);

/** Returned reference is not owned by the caller. */
GstElement      *pipeline_builder_get_pipeline(PipelineBuilder *builder);

/** New reference; caller must gst_object_unref().  Returns NULL when the element is missing. */
GstElement      *pipeline_builder_get_element(PipelineBuilder *builder,
                                               const gchar     *name);

GstElement *pipeline_builder_add_source    (PipelineBuilder *builder);
GstElement *pipeline_builder_add_h264parser(PipelineBuilder *builder);
GstElement *pipeline_builder_add_decoder   (PipelineBuilder *builder);
GstElement *pipeline_builder_add_streamux  (PipelineBuilder *builder);
GstElement *pipeline_builder_add_tracker   (PipelineBuilder *builder);
GstElement *pipeline_builder_add_nvvidconv (PipelineBuilder *builder);
GstElement *pipeline_builder_add_nvosd     (PipelineBuilder *builder);
GstElement *pipeline_builder_add_tee       (PipelineBuilder *builder);
GstElement *pipeline_builder_add_msgconv   (PipelineBuilder *builder);
GstElement *pipeline_builder_add_msgbroker (PipelineBuilder *builder);

/** Picks nv3dsink (integrated GPU) or nveglglessink (discrete) according to CUDA device. */
GstElement *pipeline_builder_add_sink(PipelineBuilder *builder);

/** config_section: YAML key passed to nvds_parse_gie (e.g. "primary-gie", "secondary-gie1"). */
GstElement *pipeline_builder_add_infer(PipelineBuilder *builder,
                                       const gchar     *element_name,
                                       const gchar     *config_section);

GstElement *pipeline_builder_add_queue(PipelineBuilder *builder,
                                       const gchar     *element_name);

#endif
