#include "pipeline_controller.h"

#include <gst/gst.h>
#include <glib.h>

struct PipelineController {
    GstElement *pipeline;
    GMainLoop  *loop;
    guint       bus_watch_id;
};

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
    PipelineController *controller = (PipelineController *) data;

    (void) bus;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(controller->loop);
            break;
        case GST_MESSAGE_ERROR: {
            gchar  *debug = NULL;
            GError *error = NULL;
            gst_message_parse_error(msg, &error, &debug);
            g_printerr("Error from element %s: %s\n",
                       GST_OBJECT_NAME(msg->src), error->message);
            if (debug)
                g_printerr("Error details: %s\n", debug);
            g_free(debug);
            g_error_free(error);
            g_main_loop_quit(controller->loop);
            break;
        }
        default:
            break;
    }

    return TRUE;
}

PipelineController *pipeline_controller_new(GstElement *pipeline)
{
    if (!pipeline)
        return NULL;

    PipelineController *controller = g_new0(PipelineController, 1);
    controller->pipeline = pipeline;
    controller->loop     = g_main_loop_new(NULL, FALSE);

    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    controller->bus_watch_id = gst_bus_add_watch(bus, bus_call, controller);
    gst_object_unref(bus);

    return controller;
}

void pipeline_controller_free(PipelineController *controller)
{
    if (!controller)
        return;

    pipeline_controller_stop(controller);

    if (controller->bus_watch_id)
        g_source_remove(controller->bus_watch_id);

    if (controller->loop)
        g_main_loop_unref(controller->loop);

    if (controller->pipeline)
        gst_object_unref(GST_OBJECT(controller->pipeline));

    g_free(controller);
}

void pipeline_controller_play(PipelineController *controller)
{
    if (controller)
        gst_element_set_state(controller->pipeline, GST_STATE_PLAYING);
}

void pipeline_controller_pause(PipelineController *controller)
{
    if (controller)
        gst_element_set_state(controller->pipeline, GST_STATE_PAUSED);
}

void pipeline_controller_stop(PipelineController *controller)
{
    if (controller)
        gst_element_set_state(controller->pipeline, GST_STATE_NULL);
}

void pipeline_controller_run_loop(PipelineController *controller)
{
    if (controller)
        g_main_loop_run(controller->loop);
}
