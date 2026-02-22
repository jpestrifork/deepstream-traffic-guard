#include <stdio.h>
#include <glib.h>

#include "gstnvdsmeta.h"

#include "probes/probe_detections.h"
#include "config.h"
#include "logger.h"

#define PGIE_CLASS_ID_VEHICLE 0
#define DETECTION_OUTPUT_FILENAME_PATTERN "frame_%06d.txt"

static gint detection_frame_counter = 0;

/* Caller must g_free() the returned string. */
static gchar *get_plate_text_from_plate_obj(NvDsObjectMeta *plate_obj)
{
    NvDsClassifierMetaList *l_class = NULL;
    NvDsLabelInfoList *l_label = NULL;

    for (l_class = plate_obj->classifier_meta_list; l_class != NULL;
         l_class = l_class->next) {
        NvDsClassifierMeta *cm = (NvDsClassifierMeta *)(l_class->data);
        if (cm->unique_component_id != 5)
            continue;
        for (l_label = cm->label_info_list; l_label != NULL;
             l_label = l_label->next) {
            NvDsLabelInfo *li = (NvDsLabelInfo *)(l_label->data);
            if (li->result_label)
                return g_strdup(li->result_label);
            return g_strdup("-");
        }
        return g_strdup("-");
    }
    return NULL;
}

/* Caller must g_free() the returned string; may return NULL when no plate is found. */
static gchar *get_plate_text_for_car(NvDsFrameMeta *frame_meta,
                                     NvDsObjectMeta *car_obj)
{
    NvDsMetaList *l_obj = NULL;
    for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
         l_obj = l_obj->next) {
        NvDsObjectMeta *obj = (NvDsObjectMeta *)(l_obj->data);
        if (obj->unique_component_id != 4)
            continue;
        if (obj->object_id != car_obj->object_id)
            continue;
        gchar *text = get_plate_text_from_plate_obj(obj);
        return text ? text : g_strdup("-");
    }
    return NULL;
}

GstPadProbeReturn probe_write_detections(GstPad *pad,
                                         GstPadProbeInfo *info,
                                         gpointer user_data)
{
    (void)pad;
    (void)user_data;

    const gchar *output_dir = config_get_detection_output_dir();
    if (!output_dir || !output_dir[0])
        return GST_PAD_PROBE_OK;

    GstBuffer *buf = (GstBuffer *)info->data;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    NvDsMetaList *l_frame = NULL;
    NvDsMetaList *l_obj = NULL;

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
         l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        detection_frame_counter++;

        gchar *filename = g_strdup_printf(DETECTION_OUTPUT_FILENAME_PATTERN,
                                          detection_frame_counter);
        gchar *path = g_build_filename(output_dir, filename, NULL);
        g_free(filename);

        FILE *fp = fopen(path, "w");
        g_free(path);
        if (!fp)
            continue;

        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
             l_obj = l_obj->next) {
            NvDsObjectMeta *obj = (NvDsObjectMeta *)(l_obj->data);
            if (obj->class_id != PGIE_CLASS_ID_VEHICLE)
                continue;
            if (obj->unique_component_id != 1)
                continue;
            gchar *plate_text = get_plate_text_for_car(frame_meta, obj);
            if (!plate_text)
                plate_text = g_strdup("-");
            fprintf(fp, "car %.6g %.6g %.6g %.6g %s\n",
                    (double)obj->rect_params.left,
                    (double)obj->rect_params.top,
                    (double)obj->rect_params.width,
                    (double)obj->rect_params.height,
                    plate_text);
            g_free(plate_text);
        }

        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
             l_obj = l_obj->next) {
            NvDsObjectMeta *obj = (NvDsObjectMeta *)(l_obj->data);
            if (obj->class_id != PGIE_CLASS_ID_VEHICLE)
                continue;
            if (obj->unique_component_id != 4)
                continue;
            gchar *plate_text = get_plate_text_from_plate_obj(obj);
            if (!plate_text)
                plate_text = g_strdup("-");
            fprintf(fp, "plate %.6g %.6g %.6g %.6g %s\n",
                    (double)obj->rect_params.left,
                    (double)obj->rect_params.top,
                    (double)obj->rect_params.width,
                    (double)obj->rect_params.height,
                    plate_text);
            g_free(plate_text);
        }

        fclose(fp);
    }

    return GST_PAD_PROBE_OK;
}
