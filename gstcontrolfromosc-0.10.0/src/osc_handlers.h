#ifndef __OSC_HANDLERS_H__
#define __OSC_HANDLERS_H__

#include <gst/gst.h>
#include <gst/controller/gstcontroller.h>

#include "gstcontrolfromosc.h"
#include "aux_functions.h"

void register_osc_handlers (GstControlFromOsc * filter);

void osc_error (int num, const char *msg, const char *path);

void osc_handlers_debug_init ();

#endif /* __OSC_HANDLERS_H__ */
