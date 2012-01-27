#ifndef __AUX_FUNCTIONS_H__
#define __AUX_FUNCTIONS_H__

#include "gstcontrolfromosc.h"

void
create_message (lo_message message, GstElement * target_elem,
    GParamSpec * elem_prop);

gint compare_lo_address (gconstpointer a, gconstpointer b);

GValue osc_value_to_g_value (gchar type, gpointer value);

void register_gvalue_transforms ();

void aux_functions_debug_init ();

#endif /* __AUX_FUNCTIONS_H__ */
