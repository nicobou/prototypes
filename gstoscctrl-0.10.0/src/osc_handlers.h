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

#ifndef __OSC_HANDLERS_H__
#define __OSC_HANDLERS_H__

#include <gst/gst.h>
#include <gst/controller/gstcontroller.h>

#include "gstoscctrl.h"
#include "aux_functions.h"

void register_osc_handlers (GstOscctrl * filter);

void osc_error (int num, const char *msg, const char *path);

void osc_handlers_debug_init ();

#endif /* __OSC_HANDLERS_H__ */
