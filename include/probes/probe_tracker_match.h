#ifndef PROBE_TRACKER_MATCH_H
#define PROBE_TRACKER_MATCH_H

#include <gst/gst.h>

/** Attach to nvvidconv sink; copies parent car object_id to plate via IoU match so both share one tracker ID. */
GstPadProbeReturn probe_match_tracker_ids(GstPad *pad,
                                          GstPadProbeInfo *info,
                                          gpointer user_data);

#endif
