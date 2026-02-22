#include "probe_base.h"
#include "logger.h"

gboolean probe_base_add_buffer_probe(
    GstElement *element,
    const char *pad_name,
    GstPadProbeReturn (*callback)(GstPad *, GstPadProbeInfo *, gpointer),
    gpointer user_data)
{
    GstPad *pad = gst_element_get_static_pad(element, pad_name);
    if (!pad) {
        log_error("probe_base: could not get pad '%s' from element '%s'",
                  pad_name, GST_ELEMENT_NAME(element));
        return FALSE;
    }
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, callback, user_data, NULL);
    gst_object_unref(pad);
    return TRUE;
}
