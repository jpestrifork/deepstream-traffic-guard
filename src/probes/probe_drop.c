#include "gstnvdsmeta.h"

#include "probes/probe_drop.h"

GstPadProbeReturn probe_drop_frame(GstPad *pad,
                                   GstPadProbeInfo *info,
                                   gpointer user_data)
{
    (void)pad;
    (void)user_data;

    GstBuffer *buf = (GstBuffer *)info->data;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    NvDsMetaList *l_frame = NULL;

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
         l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        if (frame_meta->frame_user_meta_list == NULL)
            return GST_PAD_PROBE_DROP;
    }
    return GST_PAD_PROBE_OK;
}
