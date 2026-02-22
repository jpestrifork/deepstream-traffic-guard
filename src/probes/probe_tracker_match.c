#include <math.h>
#include <glib.h>

#include "gstnvdsmeta.h"
#include "nvdsmeta.h"

#include "probes/probe_tracker_match.h"

static float iou(NvOSD_RectParams a, NvOSD_RectParams b)
{
    float x1 = fmaxf(a.left, b.left);
    float y1 = fmaxf(a.top,  b.top);
    float x2 = fminf(a.left + a.width,  b.left + b.width);
    float y2 = fminf(a.top  + a.height, b.top  + b.height);

    float iw = fmaxf(0.0f, x2 - x1);
    float ih = fmaxf(0.0f, y2 - y1);
    float intersection = iw * ih;

    float area_a = a.width * a.height;
    float area_b = b.width * b.height;
    float union_area = area_a + area_b - intersection;

    return (union_area > 0.0f) ? (intersection / union_area) : 0.0f;
}

static NvDsObjectMeta *find_parent_car(NvDsFrameMeta *frame_meta,
                                       NvDsObjectMeta *plate)
{
    NvDsMetaList *l_obj = NULL;
    float max_iou = 0.0f;
    NvDsObjectMeta *parent = NULL;

    for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
         l_obj = l_obj->next) {
        NvDsObjectMeta *obj = (NvDsObjectMeta *)(l_obj->data);
        if (obj->unique_component_id != 1)
            continue;
        float score = iou(obj->rect_params, plate->rect_params);
        if (score > max_iou) {
            max_iou = score;
            parent = obj;
        }
    }
    return (parent && max_iou > 0.0f) ? parent : NULL;
}

GstPadProbeReturn probe_match_tracker_ids(GstPad *pad,
                                          GstPadProbeInfo *info,
                                          gpointer user_data)
{
    (void)pad;
    (void)user_data;

    GstBuffer *buf = (GstBuffer *)info->data;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    NvDsMetaList *l_frame = NULL;
    NvDsMetaList *l_obj = NULL;

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
         l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
             l_obj = l_obj->next) {
            NvDsObjectMeta *obj = (NvDsObjectMeta *)(l_obj->data);
            if (obj->unique_component_id != 4)
                continue;
            NvDsObjectMeta *car = find_parent_car(frame_meta, obj);
            if (car)
                obj->object_id = car->object_id;
        }
    }
    return GST_PAD_PROBE_OK;
}
