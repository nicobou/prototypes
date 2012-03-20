/*
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011      Marcio Tomiyoshi <mmt@ime.usp.br>
 * Copyright (C) 2011-2012 Nicolas Bouillot (http://www.nicolasbouillot.net)
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

#ifndef __GST_OSCCTRL_H__
#define __GST_OSCCTRL_H__

#include <gst/gst.h>
#include <gst/controller/gstcontroller.h>
#include "lo/lo.h"

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define GST_TYPE_OSCCTRL			\
  (gst_control_from_osc_get_type())
#define GST_OSCCTRL(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OSCCTRL,GstOscctrl))
#define GST_OSCCTRL_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OSCCTRL,GstOscctrlClass))
#define GST_IS_OSCCTRL(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OSCCTRL))
#define GST_IS_OSCCTRL_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OSCCTRL))
#define DEFAULT_OSC_PORT 7770
typedef struct _GstOscctrl GstOscctrl;
typedef struct _GstOscctrlClass GstOscctrlClass;

struct _GstOscctrl
{
  GstElement element;

  GstPad *sinkpad, *srcpad;
  GstController *controller;
  GstInterpolationControlSource *control_source;
  GstInterpolateMode interpolate_mode;

  lo_server_thread osc_server;
  gint osc_port;

  GHashTable *subscribers;
};

struct _GstOscctrlClass
{
  GstElementClass parent_class;
};

GType gst_control_from_osc_get_type (void);

G_END_DECLS
#endif /* __GST_OSCCTRL_H__ */
