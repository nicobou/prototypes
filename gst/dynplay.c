#include <gst/gst.h>
#include <signal.h>

GstElement *pipeline;
GstElement *mixer;


static gboolean
event_probe_cb (GstPad *pad, GstEvent * event, gpointer data)
{
  /*   if (GST_EVENT_TYPE (event) == GST_EVENT_NEWSEGMENT) { */
  /*   gdouble rate, applied_rate; */
  /*   gdouble new_rate; */

  /*   gst_event_parse_new_segment_full (event, NULL, &rate, &applied_rate, NULL, */
  /*       NULL, NULL, NULL); */
  /*   new_rate = rate * applied_rate; */
  /*   if (priv->rate != new_rate) { */
  /*     priv->rate = new_rate; */
  /*     g_signal_emit (player, demo_player_signals[SIGNAL_RATE_CHANGE], 0, */
  /*         new_rate); */
  /*   } */
  /* } */
  
  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) { 
    g_print ("EOS caught and disabled \n");
    g_print ("pad with EOS %s:%s \n",
	     GST_DEBUG_PAD_NAME (pad));
  }
  
  return TRUE;
}



void
leave(int sig) {
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);
    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));
    exit(sig);
}

static gboolean
bus_call (GstBus *bus,
          GstMessage *msg,
          gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream, name: %s\n",
	       GST_MESSAGE_SRC_NAME(msg));
      //g_main_loop_quit (loop);
      break;
      
    case GST_MESSAGE_ERROR: {
      gchar *debug;
      GError *error;
	
	gst_message_parse_error (msg, &error, &debug);
	g_free (debug);
      
	g_printerr ("Error: %s\n", error->message);
	g_error_free (error);
      
	//g_main_loop_quit (loop);
	break;
    }
    default:
	//g_print ("unknown message type \n");
	break;
    }
    return TRUE;
}

void pad_removed_cb (GstElement* object, GstPad* pad, gpointer user_data)
{
  g_print ("pad removed %s:%s \n",
	   GST_DEBUG_PAD_NAME (pad));
}

void pad_added_cb (GstElement* object, GstPad* pad, gpointer user_data)
{
  GstElement *mixer = (GstElement *) user_data;
  GstPad *mixerSinkPad = gst_element_get_compatible_pad (mixer, pad, NULL);

  g_print ("trying to link pad %s:%s \n",
	   GST_DEBUG_PAD_NAME (pad));
  
  if (mixerSinkPad == NULL) {
    g_print ("Couldn't get a mixer channel for pad %s:%s\n",
	     GST_DEBUG_PAD_NAME (pad));
    return;
  }

  //probing eos for cleaning
  gst_pad_add_event_probe (pad, (GCallback) event_probe_cb, NULL);
  
  if (G_UNLIKELY (gst_pad_link (pad, mixerSinkPad) != GST_PAD_LINK_OK)) {
    g_print ("Couldn't link pads\n");
  }
  
  return;
}

static gboolean
autoplug_continue_cb (GstElement * uridecodebin, GstPad * somepad, GstCaps * caps, gpointer user_data)
{
  g_print ("autoplug_continue_cb \n");
  return TRUE;
}



void add_sample (GstElement *pipeline,GstElement *mixer)
{

    GstElement *sample;
    sample = gst_element_factory_make ("uridecodebin",NULL);
    g_object_set (G_OBJECT (sample), "uri", "file:///usr/share/sounds/alsa/Front_Center.wav" , NULL);  
    g_object_set (G_OBJECT (sample), "message-forward", TRUE , NULL);  

    g_signal_connect (G_OBJECT (sample), "pad-added", (GCallback) pad_added_cb , (gpointer) mixer);  

    if (!sample)  {
	g_printerr ("one element for sample handling could not be created. Exiting.\n");
    }
    else
	{
	    gst_bin_add_many (GST_BIN (pipeline),
			      sample,
			      NULL);
	    if (!gst_element_sync_state_with_parent (sample))
		g_error ("audiotestsrc problem sync with parent\n");
	}

}

static gboolean
play_sample ()
{
    g_print ("adding a new sample\n");
    add_sample (pipeline,mixer);
    return TRUE;
}


int
main (int argc,
      char *argv[])
{
    (void) signal(SIGINT,leave);
    
    /* Initialisation */
    gst_init (&argc, &argv);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    /* Create gstreamer elements */
    pipeline  = gst_pipeline_new (NULL);

    if (!pipeline) {
	g_printerr ("The pipeline could not be created. Exiting.\n");
	return -1;
    }
    
    //creating audio out
    GstElement *audiosink = gst_element_factory_make ("autoaudiosink", NULL);
    mixer = gst_element_factory_make ("adder",NULL);
    gst_bin_add_many (GST_BIN (pipeline),
		      mixer,
		      audiosink,	   
		      NULL);

    gst_element_link_many (mixer,
			   audiosink,	   
			   NULL);
    
    /* message handler */
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    //    add_sample (pipeline,mixer);
    // request new sample periodically
    g_timeout_add (350, (GSourceFunc) play_sample, NULL);

    /* Set the pipeline to "playing" state*/
    g_print ("Now playing\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Iterate */
    g_print ("Running...\n");
    g_main_loop_run (loop);

    /* Out of the main loop, clean up nicely */
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));

    return 0;
}
