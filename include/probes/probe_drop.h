#ifndef PROBE_DROP_H
#define PROBE_DROP_H

#include <gst/gst.h>

/** Attach to queue1 sink; drops buffers that carry no user meta (nothing to send to the broker branch). */
GstPadProbeReturn probe_drop_frame(GstPad *pad,
                                   GstPadProbeInfo *info,
                                   gpointer user_data);

#endif
