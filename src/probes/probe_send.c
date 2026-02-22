#include <string.h>
#include <glib.h>

#include "gstnvdsmeta.h"
#include "nvdsmeta.h"
#include "nvdsmeta_schema.h"

#include "probes/probe_send.h"

#define PGIE_CLASS_ID_VEHICLE 0

static gpointer meta_copy_func(gpointer data, gpointer user_data)
{
    (void)user_data;
    NvDsUserMeta *user_meta = (NvDsUserMeta *)data;
    NvDsCustomMsgInfo *src = (NvDsCustomMsgInfo *)user_meta->user_meta_data;
    NvDsCustomMsgInfo *dst = (NvDsCustomMsgInfo *)g_memdup2(src,
                                                             sizeof(NvDsCustomMsgInfo));
    if (src->message)
        dst->message = (gpointer)g_strdup((const char *)src->message);
    dst->size = src->size;
    return dst;
}

static void meta_free_func(gpointer data, gpointer user_data)
{
    (void)user_data;
    NvDsUserMeta *user_meta = (NvDsUserMeta *)data;
    NvDsCustomMsgInfo *src = (NvDsCustomMsgInfo *)user_meta->user_meta_data;
    if (src->message)
        g_free(src->message);
    src->size = 0;
    g_free(user_meta->user_meta_data);
}

static gchar *build_payload(guint64 id, gchar *brand, gchar *type,
                             gchar *plate)
{
    gchar *safe_id    = g_strdup_printf("%" G_GUINT64_FORMAT, id);
    const gchar *sb   = brand ? brand : "NULL";
    const gchar *st   = type  ? type  : "NULL";
    const gchar *sp   = plate ? plate : "NULL";
    gchar *payload = g_strconcat("Vehicle ID: ", safe_id,
                                 ", Brand: ", sb,
                                 ", Type: ", st,
                                 ", Plate: ", sp, NULL);
    g_free(safe_id);
    return payload;
}

GstPadProbeReturn probe_send(GstPad *pad,
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

            if (obj->class_id != PGIE_CLASS_ID_VEHICLE)
                continue;
            if (obj->unique_component_id != 1 && obj->unique_component_id != 4)
                continue;

            gchar *brand = NULL, *type = NULL, *plate = NULL;
            NvDsClassifierMetaList *l_class = NULL;
            for (l_class = obj->classifier_meta_list; l_class != NULL;
                 l_class = l_class->next) {
                NvDsClassifierMeta *cm = (NvDsClassifierMeta *)(l_class->data);
                NvDsLabelInfoList *l_label = NULL;
                for (l_label = cm->label_info_list; l_label != NULL;
                     l_label = l_label->next) {
                    NvDsLabelInfo *li = (NvDsLabelInfo *)(l_label->data);
                    switch (cm->unique_component_id) {
                        case 2: brand = g_strdup(li->result_label); break;
                        case 3: type  = g_strdup(li->result_label); break;
                        case 5: plate = g_strdup(li->result_label); break;
                        default: break;
                    }
                }
            }

            NvDsUserMeta *user_meta =
                nvds_acquire_user_meta_from_pool(batch_meta);
            NvDsCustomMsgInfo *msg =
                (NvDsCustomMsgInfo *)g_malloc0(sizeof(NvDsCustomMsgInfo));

            gchar *payload = build_payload(obj->object_id, brand, type, plate);
            msg->size    = (guint)strlen(payload);
            msg->message = g_strdup(payload);
            g_free(payload);

            if (user_meta) {
                user_meta->user_meta_data = (void *)msg;
                user_meta->base_meta.meta_type    = NVDS_CUSTOM_MSG_BLOB;
                user_meta->base_meta.copy_func    =
                    (NvDsMetaCopyFunc)meta_copy_func;
                user_meta->base_meta.release_func =
                    (NvDsMetaReleaseFunc)meta_free_func;
                nvds_add_user_meta_to_frame(frame_meta, user_meta);
            } else {
                g_free(msg->message);
                g_free(msg);
            }

            g_free(brand);
            g_free(type);
            g_free(plate);
        }
    }
    return GST_PAD_PROBE_OK;
}
