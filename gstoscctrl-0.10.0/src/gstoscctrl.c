/*
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
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

/**
 * SECTION:element-oscctrl
 *
 * oscctrl allows control of elements properties of a pipeline from OSC messages.
 *
 * Set a new value of an element property
 * |[
 * /set ss* <element_name> <property> <new_value>
 * /set/element_name/property * <new_value>
 * ]|
 * Get the value of an element property
 * |[
 * /get ssss <element_name> <property> <reply_address> <reply_port>
 * /get/element_name/propery ss <reply_address> <reply_port>
 * }|
 *
 * Being notified when an element has his property changed
 * |[
 * /subscribe ssss <element_name> <property> <reply_address> <reply_port>
 * /unsubscribe ssss <element_name> <property> <reply_address> <reply_port>
 * /subscribe/element_name/property ss <reply_address> <reply_port>
 * /unsubscribe/element_name/property ss <reply_address> <reply_port>
 * ]|
 *
 * Relative automation (starting when message is received)
 * |[
 * /control ss*d <element_name> <property> <new_value> <time_to_reach_value>
 * /control/element_name/property *d <new_value> <time_to_reach_value>
 * ]|
 *
 * Seeking the pipeline
 * |[ 
 * /locate d <absolute_time_in_seconds>
 * ]|
 * Pipeline states
 * |[
 * /pause
 * /play
 * ]|
 *
 *
 *  gst-launch --gst-plugin-path=/usr/local/lib/gstreamer-0.10/ oscctrl videotestsrc ! timeoverlay ! xvimagesink audiotestsrc ! autoaudiosink
 *
 *
 * you can use oscsend and osc dump in order to interact with the pipeline, 
 * for instance:
 *
 * oscdump 8888
 *
 * oscsend localhost 7770  /subscribe ssss audiotestsrc0 freq localhost 8888
 * oscsend localhost 7770  /subscribe ssss audiotestsrc0 freq localhost 8888
 * oscsend localhost 7770 /set ssi videotestsrc0 pattern 10
 * oscsend localhost 7770 /set/videotestsrc0/pattern i 0
 * oscsend localhost 7770 /control ssid audiotestsrc0 freq 100 3
 * oscsend localhost 7770 /set/oscctrl0/linear-interp s true
 * oscsend localhost 7770 /control ssid audiotestsrc0 freq 440 1
 * oscsend localhost 7770 /locate d 10.9
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m oscctrl port=7770 fakesrc ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstoscctrl.h"
#include "osc_handlers.h"
#include "aux_functions.h"

GST_DEBUG_CATEGORY_STATIC (gst_control_from_osc_debug);
#define GST_CAT_DEFAULT gst_control_from_osc_debug

/* Filter signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_OSC_PORT,   // Port to listen
  PROP_INTERP_MODE // Interpolation mode used on GstControlSource
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

GST_BOILERPLATE (GstOscctrl, gst_control_from_osc, GstElement,
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
      "Open Sound Control (OSC) Controller",
      "OSC",
      "Allows control of elements properties and pipeline with OSC messages.",
      "Nicolas Bouillot <<http://www.nicolasbouillot.net>>\n\
       Marcio Tomiyoshi <<mmt@ime.usp.br>>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the oscctrl's class */
static void
gst_control_from_osc_class_init (GstOscctrlClass * klass)
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
      g_param_spec_boolean ("linear-interp", "Enabling of linear interpolation",
          "Linear interpolation on/off.", TRUE,
           G_PARAM_READWRITE));
}

void
create_osc_server (GstOscctrl * filter)
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
gst_control_from_osc_init (GstOscctrl * filter,
    GstOscctrlClass * gclass)
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

  create_osc_server (filter);
}

static void
gst_control_from_osc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOscctrl *filter = GST_OSCCTRL (object);

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
  GstOscctrl *filter = GST_OSCCTRL (object);

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


/* this function handles the link with other elements */
static gboolean
gst_control_from_osc_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOscctrl *filter;
  GstPad *otherpad;

  filter = GST_OSCCTRL (gst_pad_get_parent (pad));
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
  GstOscctrl *filter;

  filter = GST_OSCCTRL (GST_OBJECT_PARENT (pad));

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (filter->srcpad, buf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
oscctrl_init (GstPlugin * oscctrl)
{
  /* debug category for filtering log messages
   *
   * exchange the string 'Template oscctrl' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_control_from_osc_debug, "oscctrl",
      0, "Template oscctrl");
  aux_functions_debug_init ();
  osc_handlers_debug_init ();

  return gst_element_register (oscctrl, "oscctrl", GST_RANK_NONE,
      GST_TYPE_OSCCTRL);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "oscctrl"
#endif

/* gstreamer looks for this structure to register oscctrls */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "oscctrl",
    "Allows control of elements properties of a pipeline from OSC messages.",
    oscctrl_init, VERSION, "LGPL", "GStreamer", "http://gstreamer.net/")
