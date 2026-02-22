#ifndef PROBE_SEND_H
#define PROBE_SEND_H

#include <gst/gst.h>

/** Attach to nvosd sink; attaches NVDS_CUSTOM_MSG_BLOB payload for the message broker. */
GstPadProbeReturn probe_send(GstPad *pad,
                             GstPadProbeInfo *info,
                             gpointer user_data);

#endif
