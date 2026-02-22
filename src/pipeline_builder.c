#include <gst/gst.h>
#include <cuda_runtime_api.h>

#include "nvds_yml_parser.h"
#include "pipeline_builder.h"
#include "logger.h"

struct PipelineBuilder {
    GstElement *pipeline;
    char       *config_path;
};

PipelineBuilder *pipeline_builder_new(const char *config_path)
{
    PipelineBuilder *builder = g_new0(PipelineBuilder, 1);

    builder->pipeline = gst_pipeline_new("deepstream-app");
    if (!builder->pipeline) {
        log_error("Failed to create GStreamer pipeline");
        g_free(builder);
        return NULL;
    }

    builder->config_path = g_strdup(config_path);
    return builder;
}

void pipeline_builder_free(PipelineBuilder *builder)
{
    if (!builder)
        return;
    if (builder->pipeline)
        gst_object_unref(GST_OBJECT(builder->pipeline));
    g_free(builder->config_path);
    g_free(builder);
}

GstElement *pipeline_builder_get_pipeline(PipelineBuilder *builder)
{
    return builder ? builder->pipeline : NULL;
}

GstElement *pipeline_builder_get_element(PipelineBuilder *builder,
                                          const gchar     *name)
{
    if (!builder || !builder->pipeline || !name)
        return NULL;
    return gst_bin_get_by_name(GST_BIN(builder->pipeline), name);
}

static GstElement *make_and_add(PipelineBuilder *builder,
                                 const gchar     *factory,
                                 const gchar     *element_name)
{
    GstElement *elem = gst_element_factory_make(factory, element_name);
    if (!elem) {
        g_printerr("Failed to create element '%s' (factory '%s')\n",
                   element_name, factory);
        return NULL;
    }
    gst_bin_add(GST_BIN(builder->pipeline), elem);
    return elem;
}

GstElement *pipeline_builder_add_source(PipelineBuilder *builder)
{
    GstElement *elem = make_and_add(builder, "filesrc", "file-source");
    if (elem)
        nvds_parse_file_source(elem, builder->config_path, "source");
    return elem;
}

GstElement *pipeline_builder_add_h264parser(PipelineBuilder *builder)
{
    return make_and_add(builder, "h264parse", "h264-parser");
}

GstElement *pipeline_builder_add_decoder(PipelineBuilder *builder)
{
    return make_and_add(builder, "nvv4l2decoder", "nvv4l2-decoder");
}

GstElement *pipeline_builder_add_streamux(PipelineBuilder *builder)
{
    GstElement *elem = make_and_add(builder, "nvstreammux", "muxer");
    if (elem)
        nvds_parse_streammux(elem, builder->config_path, "streammux");
    return elem;
}

GstElement *pipeline_builder_add_tracker(PipelineBuilder *builder)
{
    GstElement *elem = make_and_add(builder, "nvtracker", "tracker");
    if (elem)
        nvds_parse_tracker(elem, builder->config_path, "tracker");
    return elem;
}

GstElement *pipeline_builder_add_nvvidconv(PipelineBuilder *builder)
{
    return make_and_add(builder, "nvvideoconvert", "nvvideo-converter");
}

GstElement *pipeline_builder_add_nvosd(PipelineBuilder *builder)
{
    return make_and_add(builder, "nvdsosd", "on-screen-display");
}

GstElement *pipeline_builder_add_tee(PipelineBuilder *builder)
{
    return make_and_add(builder, "tee", "tee");
}

GstElement *pipeline_builder_add_msgconv(PipelineBuilder *builder)
{
    GstElement *elem = make_and_add(builder, "nvmsgconv", "nvmsg-converter");
    if (elem) {
        g_object_set(G_OBJECT(elem), "config", "msgconv_config.yml", NULL);
        nvds_parse_msgconv(elem, builder->config_path, "msgconv");
    }
    return elem;
}

GstElement *pipeline_builder_add_msgbroker(PipelineBuilder *builder)
{
    GstElement *elem = make_and_add(builder, "nvmsgbroker", "msg-broker");
    if (elem)
        nvds_parse_msgbroker(elem, builder->config_path, "msgbroker");
    return elem;
}

GstElement *pipeline_builder_add_sink(PipelineBuilder *builder)
{
    int current_device = -1;
    cudaGetDevice(&current_device);
    struct cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, current_device);

    if (prop.integrated)
        return make_and_add(builder, "nv3dsink",      "nv3d-sink");
    else
        return make_and_add(builder, "nveglglessink", "nvvideo-renderer");
}

GstElement *pipeline_builder_add_infer(PipelineBuilder *builder,
                                        const gchar     *element_name,
                                        const gchar     *config_section)
{
    GstElement *elem = make_and_add(builder, "nvinfer", element_name);
    if (elem)
        nvds_parse_gie(elem, builder->config_path, config_section);
    return elem;
}

GstElement *pipeline_builder_add_queue(PipelineBuilder *builder,
                                        const gchar     *element_name)
{
    return make_and_add(builder, "queue", element_name);
}
