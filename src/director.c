#include <glib.h>

#include "director.h"
#include "pipeline_builder.h"
#include "pipeline_linker.h"
#include "probe_base.h"
#include "probes/probe_send.h"
#include "probes/probe_detections.h"
#include "probes/probe_tracker_match.h"
#include "probes/probe_drop.h"
#include "config.h"
#include "logger.h"

GstElement *director_build(const char *config_path)
{
    PipelineBuilder *builder = pipeline_builder_new(config_path);
    if (!builder) {
        log_error("director: failed to create PipelineBuilder");
        return NULL;
    }

    if (!pipeline_builder_add_source(builder))     goto fail;
    if (!pipeline_builder_add_h264parser(builder)) goto fail;
    if (!pipeline_builder_add_decoder(builder))    goto fail;
    if (!pipeline_builder_add_streamux(builder))   goto fail;
    if (!pipeline_builder_add_infer(builder, "primary-inference",     "primary-gie"))   goto fail;
    if (!pipeline_builder_add_infer(builder, "secondary-inference-1", "secondary-gie1")) goto fail;
    if (!pipeline_builder_add_infer(builder, "secondary-inference-2", "secondary-gie2")) goto fail;
    if (!pipeline_builder_add_infer(builder, "secondary-inference-3", "secondary-gie3")) goto fail;
    if (!pipeline_builder_add_infer(builder, "secondary-inference-4", "secondary-gie4")) goto fail;
    if (!pipeline_builder_add_tracker(builder))    goto fail;
    if (!pipeline_builder_add_nvvidconv(builder))  goto fail;
    if (!pipeline_builder_add_nvosd(builder))      goto fail;
    if (!pipeline_builder_add_tee(builder))        goto fail;
    if (!pipeline_builder_add_queue(builder, "queue1")) goto fail;
    if (!pipeline_builder_add_queue(builder, "queue2")) goto fail;
    if (!pipeline_builder_add_msgconv(builder))    goto fail;
    if (!pipeline_builder_add_msgbroker(builder))  goto fail;
    if (!pipeline_builder_add_sink(builder))       goto fail;

    if (!pipeline_linker_link(builder)) {
        log_error("director: pipeline linking failed");
        goto fail;
    }

    {
        const gchar *dir = config_get_detection_output_dir();
        if (dir && dir[0]) {
            if (g_mkdir_with_parents(dir, 0755) != 0)
                log_warning("director: could not create detection output dir %s", dir);
        }
    }

    {
        GstElement *nvosd     = pipeline_builder_get_element(builder, "on-screen-display");
        GstElement *nvvidconv = pipeline_builder_get_element(builder, "nvvideo-converter");
        GstElement *queue1    = pipeline_builder_get_element(builder, "queue1");

        if (!nvosd || !nvvidconv || !queue1) {
            log_error("director: could not retrieve elements for probe attachment");
            if (nvosd)     gst_object_unref(nvosd);
            if (nvvidconv) gst_object_unref(nvvidconv);
            if (queue1)    gst_object_unref(queue1);
            goto fail;
        }

        probe_base_add_buffer_probe(nvosd,     "sink", probe_send,              NULL);
        probe_base_add_buffer_probe(nvosd,     "sink", probe_write_detections,  NULL);
        probe_base_add_buffer_probe(nvvidconv, "sink", probe_match_tracker_ids, NULL);
        probe_base_add_buffer_probe(queue1,    "sink", probe_drop_frame,        NULL);

        gst_object_unref(nvosd);
        gst_object_unref(nvvidconv);
        gst_object_unref(queue1);
    }

    GstElement *pipeline = pipeline_builder_get_pipeline(builder);
    gst_object_ref(pipeline);
    /* Caller holds the ref; pipeline_builder_free leaves the bin intact. */
    pipeline_builder_free(builder);

    return pipeline;

fail:
    pipeline_builder_free(builder);
    return NULL;
}
