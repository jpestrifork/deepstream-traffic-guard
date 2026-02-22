#ifndef PROBE_DETECTIONS_H
#define PROBE_DETECTIONS_H

#include <gst/gst.h>

/** Attach to nvosd sink; writes .txt files under config_get_detection_output_dir(). */
GstPadProbeReturn probe_write_detections(GstPad *pad,
                                         GstPadProbeInfo *info,
                                         gpointer user_data);

#endif
