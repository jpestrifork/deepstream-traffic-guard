#ifndef PROBE_BASE_H
#define PROBE_BASE_H

#include <gst/gst.h>

/** Registers a buffer probe on the static pad (pad unreffed after registration).  TRUE on success. */
gboolean probe_base_add_buffer_probe(
    GstElement *element,
    const char *pad_name,
    GstPadProbeReturn (*callback)(GstPad *, GstPadProbeInfo *, gpointer),
    gpointer user_data);

#endif
