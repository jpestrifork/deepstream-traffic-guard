#ifndef PIPELINE_LINKER_H
#define PIPELINE_LINKER_H

#include <gst/gst.h>
#include "pipeline_builder.h"

/**
 * Links all elements built by the builder.  TRUE on success, FALSE on link or
 * pad-request failure.  Request pads are used for streamux (sink_0) and tee (src_%u).
 */
gboolean pipeline_linker_link(PipelineBuilder *builder);

#endif
