#ifndef DIRECTOR_H
#define DIRECTOR_H

#include <gst/gst.h>

/**
 * Caller owns the reference and must call
 * gst_object_unref() when done; NULL on failure.
 */
GstElement *director_build(const char *config_path);

#endif
