#include <gst/gst.h>

#include "pipeline_builder.h"
#include "pipeline_linker.h"
#include "logger.h"

/* Caller owns the returned reference and must gst_object_unref() it; logs and returns NULL when the element is missing. */
static GstElement *get_elem(PipelineBuilder *builder, const gchar *name)
{
    GstElement *elem = pipeline_builder_get_element(builder, name);
    if (!elem)
        log_error("pipeline_linker: element '%s' not found in pipeline", name);
    return elem;
}

gboolean pipeline_linker_link(PipelineBuilder *builder)
{
    gboolean ret = FALSE;

    /* Refs come from gst_bin_get_by_name; cleanup must unref every element. */
    GstElement *source    = get_elem(builder, "file-source");
    GstElement *h264parser= get_elem(builder, "h264-parser");
    GstElement *decoder   = get_elem(builder, "nvv4l2-decoder");
    GstElement *streamux  = get_elem(builder, "muxer");
    GstElement *pgie      = get_elem(builder, "primary-inference");
    GstElement *nvtracker = get_elem(builder, "tracker");
    GstElement *sgie1     = get_elem(builder, "secondary-inference-1");
    GstElement *sgie2     = get_elem(builder, "secondary-inference-2");
    GstElement *sgie3     = get_elem(builder, "secondary-inference-3");
    GstElement *sgie4     = get_elem(builder, "secondary-inference-4");
    GstElement *nvvidconv = get_elem(builder, "nvvideo-converter");
    GstElement *nvosd     = get_elem(builder, "on-screen-display");
    GstElement *tee       = get_elem(builder, "tee");
    GstElement *queue1    = get_elem(builder, "queue1");
    GstElement *queue2    = get_elem(builder, "queue2");
    GstElement *msgconv   = get_elem(builder, "nvmsg-converter");
    GstElement *msgbroker = get_elem(builder, "msg-broker");

    /* Sink element name varies by GPU: nv3d-sink (integrated) or nvvideo-renderer (discrete). */
    GstElement *sink = pipeline_builder_get_element(builder, "nv3d-sink");
    if (!sink)
        sink = pipeline_builder_get_element(builder, "nvvideo-renderer");
    if (!sink)
        log_error("pipeline_linker: neither 'nv3d-sink' nor 'nvvideo-renderer' found");

    if (!source || !h264parser || !decoder || !streamux || !pgie || !nvtracker ||
        !sgie1  || !sgie2     || !sgie3   || !sgie4    ||
        !nvvidconv || !nvosd  || !tee     ||
        !queue1 || !queue2    || !msgconv || !msgbroker || !sink)
        goto cleanup;

    if (!gst_element_link_many(source, h264parser, decoder, NULL)) {
        log_error("pipeline_linker: failed to link source → h264parser → decoder");
        goto cleanup;
    }

    /* streamux uses request pads; obtain sink_0 and link decoder to it. */
    {
        GstPad *sinkpad = gst_element_request_pad_simple(streamux, "sink_0");
        if (!sinkpad) {
            log_error("pipeline_linker: failed to request sink_0 from streamux");
            goto cleanup;
        }
        GstPad *srcpad = gst_element_get_static_pad(decoder, "src");
        if (!srcpad) {
            log_error("pipeline_linker: failed to get src pad from decoder");
            gst_object_unref(sinkpad);
            goto cleanup;
        }
        GstPadLinkReturn link_ret = gst_pad_link(srcpad, sinkpad);
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);
        if (link_ret != GST_PAD_LINK_OK) {
            log_error("pipeline_linker: failed to link decoder → streamux");
            goto cleanup;
        }
    }

    if (!gst_element_link_many(streamux, pgie, nvtracker,
                                sgie1, sgie2, sgie3, sgie4,
                                nvvidconv, nvosd, tee, NULL)) {
        log_error("pipeline_linker: failed to link inference chain");
        goto cleanup;
    }

    /* tee uses request pads; obtain two src pads and link them to queue1 and queue2. */
    {
        GstPad *tee_msg_pad    = gst_element_request_pad_simple(tee, "src_%u");
        GstPad *tee_render_pad = gst_element_request_pad_simple(tee, "src_%u");

        if (!tee_msg_pad || !tee_render_pad) {
            log_error("pipeline_linker: failed to request src pads from tee");
            if (tee_msg_pad)    gst_object_unref(tee_msg_pad);
            if (tee_render_pad) gst_object_unref(tee_render_pad);
            goto cleanup;
        }

        GstPad *queue1_pad = gst_element_get_static_pad(queue1, "sink");
        if (!queue1_pad) {
            log_error("pipeline_linker: failed to get sink pad from queue1");
            gst_object_unref(tee_msg_pad);
            gst_object_unref(tee_render_pad);
            goto cleanup;
        }
        GstPadLinkReturn r1 = gst_pad_link(tee_msg_pad, queue1_pad);
        gst_object_unref(tee_msg_pad);
        gst_object_unref(queue1_pad);
        if (r1 != GST_PAD_LINK_OK) {
            log_error("pipeline_linker: failed to link tee → queue1");
            gst_object_unref(tee_render_pad);
            goto cleanup;
        }

        GstPad *queue2_pad = gst_element_get_static_pad(queue2, "sink");
        if (!queue2_pad) {
            log_error("pipeline_linker: failed to get sink pad from queue2");
            gst_object_unref(tee_render_pad);
            goto cleanup;
        }
        GstPadLinkReturn r2 = gst_pad_link(tee_render_pad, queue2_pad);
        gst_object_unref(tee_render_pad);
        gst_object_unref(queue2_pad);
        if (r2 != GST_PAD_LINK_OK) {
            log_error("pipeline_linker: failed to link tee → queue2");
            goto cleanup;
        }
    }

    if (!gst_element_link_many(queue1, msgconv, msgbroker, NULL)) {
        log_error("pipeline_linker: failed to link queue1 → msgconv → msgbroker");
        goto cleanup;
    }
    if (!gst_element_link_many(queue2, sink, NULL)) {
        log_error("pipeline_linker: failed to link queue2 → sink");
        goto cleanup;
    }

    ret = TRUE;

cleanup:
    /* Release every element reference acquired above. */
    if (source)     gst_object_unref(source);
    if (h264parser) gst_object_unref(h264parser);
    if (decoder)    gst_object_unref(decoder);
    if (streamux)   gst_object_unref(streamux);
    if (pgie)       gst_object_unref(pgie);
    if (nvtracker)  gst_object_unref(nvtracker);
    if (sgie1)      gst_object_unref(sgie1);
    if (sgie2)      gst_object_unref(sgie2);
    if (sgie3)      gst_object_unref(sgie3);
    if (sgie4)      gst_object_unref(sgie4);
    if (nvvidconv)  gst_object_unref(nvvidconv);
    if (nvosd)      gst_object_unref(nvosd);
    if (tee)        gst_object_unref(tee);
    if (queue1)     gst_object_unref(queue1);
    if (queue2)     gst_object_unref(queue2);
    if (msgconv)    gst_object_unref(msgconv);
    if (msgbroker)  gst_object_unref(msgbroker);
    if (sink)       gst_object_unref(sink);

    return ret;
}
