/*
 * Copyright (C) 2011-2012 Nicolas Bouillot (http://www.nicolasbouillot.net)
 * Copyright (C) 2011      Marcio Tomiyoshi <mmt@ime.usp.br>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

#ifndef __AUX_FUNCTIONS_H__
#define __AUX_FUNCTIONS_H__

#include "gstoscctrl.h"

void
create_message (lo_message message, GstElement * target_elem,
		GParamSpec * elem_prop);

gint compare_lo_address (gconstpointer a, gconstpointer b);

GValue osc_value_to_g_value (gchar type, gpointer value);

void register_gvalue_transforms ();

void aux_functions_debug_init ();

#endif /* __AUX_FUNCTIONS_H__ */
