/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2010 Nicolas Bouillot <<nicolas@cim.mcgill.ca>>
 * Copyright (C) 2011 Marcio Tomiyoshi <<mmt@ime.usp.br>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-controlfromosc
 *
 * controlfromosc allows manipulation of the GStreamer pipeline from OSC
 * messages.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! controlfromosc port=7770 ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstcontrolfromosc.h"
#include "osc_handlers.h"
#include "aux_functions.h"

GST_DEBUG_CATEGORY_STATIC (gst_control_from_osc_debug);
#define GST_CAT_DEFAULT gst_control_from_osc_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_OSC_PORT,                // Port to listen
  PROP_INTERP_MODE              // Interpolation mode used on GstControlSource
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GST_BOILERPLATE (GstControlFromOsc, gst_control_from_osc, GstElement,
    GST_TYPE_ELEMENT);

static void gst_control_from_osc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_control_from_osc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_control_from_osc_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_control_from_osc_chain (GstPad * pad, GstBuffer * buf);

/* GObject vmethod implementations */

static void
gst_control_from_osc_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "ControlFromOsc",
      "OSC",
      "Allows manipulation of the GStreamer pipeline from OSC messages.",
      "Nicolas Bouillot <<nicolas@cim.mcgill.ca>>\n\
       Marcio Tomiyoshi <<mmt@ime.usp.br>>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the controlfromosc's class */
static void
gst_control_from_osc_class_init (GstControlFromOscClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_control_from_osc_set_property;
  gobject_class->get_property = gst_control_from_osc_get_property;

  g_object_class_install_property (gobject_class, PROP_OSC_PORT,
      g_param_spec_int ("port", "Port", "OSC port to listen.",
          1024, G_MAXINT, DEFAULT_OSC_PORT, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_INTERP_MODE,
      g_param_spec_boolean ("interp-mode", "Interpolation Mode",
          "Linear interpolation on /control on or off.", FALSE,
           G_PARAM_READWRITE));
}

void
create_osc_server (GstControlFromOsc * filter)
{
  GST_INFO ("Creating OSC server on port %d", filter->osc_port);

  if (filter->osc_server != NULL) {
    GST_INFO ("Destroying old server...");
    lo_server_thread_free (filter->osc_server);
  }

  gchar *oscport_str = g_strdup_printf ("%d", filter->osc_port);
  filter->osc_server = lo_server_thread_new (oscport_str, osc_error);
  g_free (oscport_str);

  register_osc_handlers (filter);

  GST_INFO ("Starting server thread...");
  lo_server_thread_start (filter->osc_server);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_control_from_osc_init (GstControlFromOsc * filter,
    GstControlFromOscClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_control_from_osc_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_control_from_osc_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->osc_server = NULL;
  filter->osc_port = DEFAULT_OSC_PORT;
  filter->controller = NULL;
  filter->control_source = NULL;
  filter->interpolate_mode = GST_INTERPOLATE_NONE;
  filter->subscribers = g_hash_table_new (g_str_hash, g_str_equal);
  
  gst_controller_init (NULL, NULL);

  // Allows GValue transformations from strings to int, uint, float...
  register_gvalue_transforms ();

  //using default "g_direct_hash()"
/*    filter->elementsbyname= g_hash_table_new (g_str_hash,g_str_equal);*/

  create_osc_server (filter);
}

static void
gst_control_from_osc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstControlFromOsc *filter = GST_CONTROLFROMOSC (object);

  switch (prop_id) {
    case PROP_OSC_PORT:
      filter->osc_port = g_value_get_int (value);
      create_osc_server (filter);
      break;
    case PROP_INTERP_MODE:{
      gboolean b = g_value_get_boolean (value);
      if (b)
        filter->interpolate_mode = GST_INTERPOLATE_LINEAR;
      else
        filter->interpolate_mode = GST_INTERPOLATE_NONE;
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_control_from_osc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstControlFromOsc *filter = GST_CONTROLFROMOSC (object);

  switch (prop_id) {
    case PROP_OSC_PORT:
      g_value_set_int (value, filter->osc_port);
      break;
    case PROP_INTERP_MODE:
      g_value_set_boolean (value, filter->interpolate_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_control_from_osc_set_caps (GstPad * pad, GstCaps * caps)
{
  GstControlFromOsc *filter;
  GstPad *otherpad;

  filter = GST_CONTROLFROMOSC (gst_pad_get_parent (pad));
  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_control_from_osc_chain (GstPad * pad, GstBuffer * buf)
{
  GstControlFromOsc *filter;

  filter = GST_CONTROLFROMOSC (GST_OBJECT_PARENT (pad));

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (filter->srcpad, buf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
controlfromosc_init (GstPlugin * controlfromosc)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template controlfromosc' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_control_from_osc_debug, "controlfromosc",
      0, "Template controlfromosc");
  aux_functions_debug_init ();
  osc_handlers_debug_init ();

  return gst_element_register (controlfromosc, "controlfromosc", GST_RANK_NONE,
      GST_TYPE_CONTROLFROMOSC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstcontrolfromosc"
#endif

/* gstreamer looks for this structure to register controlfromoscs */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "controlfromosc",
    "Allows manipulation of the GStreamer pipeline from OSC messages.",
    controlfromosc_init, VERSION, "LGPL", "GStreamer", "http://gstreamer.net/")
