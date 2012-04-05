#include <gst/gst.h>
#include <signal.h>

static GstElement *pipeline;
static GstElement *mixer;


typedef struct
{
  GstElement *bin, *src, *audioconvert, *pitch, *resample, *mixer, *pipeline;
  GstPad *src_srcpad;
  GstPad *resample_sinkpad, *resample_srcpad;
  GstPad *mixer_sinkpad;
  GstPad *bin_srcpad;
  gfloat speedRatio;
} SourceInfo;


/* remove the source from the pipeline after removing it from adder */
static gboolean
remove_sample (SourceInfo * info)
{

  if (GST_IS_ELEMENT (info->bin))
    {
      g_print ("remove source\n");
      
      /* lock the state so that we can put it to NULL without the parent messing
       * with our state */
      gst_element_set_locked_state (info->bin, TRUE);

      /* unlink bin from mixer */
      gst_pad_unlink (info->bin_srcpad, info->mixer_sinkpad);
      /* give back the pad */
      gst_element_release_request_pad (info->mixer, info->mixer_sinkpad);
      
      /* first stop the source. Remember that this might block when in the PAUSED
       * state. Alternatively one could send EOS to the source, install an event
       * probe and schedule a state change/unlink/release from the mainthread. */
      /* NOTE that the source inside the bin will emit EOS but it will not reach
       * adder because the element after the source is shut down first. We will send
       * EOS later */
      gst_element_set_state (info->bin, GST_STATE_NULL);
      
      /* remove from the bin */
      gst_bin_remove (GST_BIN (pipeline), info->bin);
      
      /* release pads */
      if (info->src_srcpad != NULL)
	gst_object_unref (info->src_srcpad);
      if (info->resample_srcpad != NULL)
	gst_object_unref (info->resample_srcpad);
      if (info->resample_sinkpad != NULL)
	gst_object_unref (info->resample_sinkpad);
      
      
      if (info->mixer_sinkpad != NULL)
	gst_object_unref (info->mixer_sinkpad);
      
      g_free (info);
    }
  return FALSE;
}


static gboolean
event_probe_cb (GstPad *pad, GstEvent * event, gpointer data)
{
  SourceInfo *info = (SourceInfo *)data;

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

    //removing source in order to avoid mixer (adder) waiting for not arriving data     
      g_idle_add((GSourceFunc)remove_sample, info);
    //do not send EOS to mixer
    return FALSE;
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
  SourceInfo *info = (SourceInfo *) user_data;
  g_print ("pad_added_cb\n");


  //probing eos for cleaning
  gst_pad_add_event_probe (pad, (GCallback) event_probe_cb, (gpointer)info);
  
  //linking newly created pad with the resample_sinkpad -- FIXME should verify compatibility  
  gst_pad_link (pad, info->resample_sinkpad);
  
  //linking bin with mixer
  // get new pad from adder, adder will now wait for data on this pad
  // assuming this is a request pad
  info->mixer_sinkpad = gst_element_get_compatible_pad (info->mixer, info->bin_srcpad, NULL);
  g_print ("trying to link pad %s:%s \n",
	   GST_DEBUG_PAD_NAME (pad));
  if (info->mixer_sinkpad == NULL) {
    g_print ("Couldn't get a mixer channel for pad %s:%s\n",
	     GST_DEBUG_PAD_NAME (pad));
    return;
  }
  gst_pad_link (info->bin_srcpad, info->mixer_sinkpad);

  return;
}

static gboolean
autoplug_continue_cb (GstElement * uridecodebin, GstPad * somepad, GstCaps * caps, gpointer user_data)
{
  g_print ("autoplug_continue_cb \n");
  return TRUE;
}


static SourceInfo *
add_sample (GstElement *pipeline, GstElement *mixer, gfloat speedRatio)
{
  SourceInfo *info;

  info = g_new0 (SourceInfo, 1);
  info->speedRatio = speedRatio;
  info->mixer = mixer;
  info->pipeline = pipeline;
  /* make source with unique name */
  info->bin = gst_element_factory_make ("bin", NULL);
  info->src = gst_element_factory_make ("uridecodebin",NULL);
  //  info->audioconvert = gst_element_factory_make ("audioconvert",NULL);
  //info->pitch = gst_element_factory_make ("pitch",NULL);
  info->resample = gst_element_factory_make ("audioresample", NULL);

  //  g_object_set (info->resample, "panorama", speedRatio, NULL);
  g_object_set (G_OBJECT (info->src), "uri", "file:///usr/share/sounds/alsa/Front_Center.wav" , NULL);  
  g_signal_connect (G_OBJECT (info->src), "pad-added", (GCallback) pad_added_cb , (gpointer) info);  

  /* add to the bin */
  gst_bin_add (GST_BIN (info->bin), info->src);
  gst_bin_add (GST_BIN (info->bin), info->resample);

  /* get pads from the elements */
  info->src_srcpad = gst_element_get_static_pad (info->src, "src");
  info->resample_srcpad = gst_element_get_static_pad (info->resample, "src");
  info->resample_sinkpad = gst_element_get_static_pad (info->resample, "sink");



  /* create and add a pad for the bin */
  info->bin_srcpad = gst_ghost_pad_new ("src", info->resample_srcpad);
  gst_element_add_pad (info->bin, info->bin_srcpad);

  gst_bin_add (GST_BIN (info->pipeline), info->bin);

  gst_element_set_state (info->bin,GST_STATE_READY);

  /* and play */
  if (!gst_element_sync_state_with_parent (info->bin))
    g_error ("audiotestsrc problem sync with parent\n");


  

  g_print ("add_sample returns\n");

  return info;
}


static gboolean
play_sample ()
{
    g_print ("adding a new sample\n");
    add_sample (pipeline,mixer,0.5);
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
    GstElement *zeroAudio = gst_element_factory_make ("audiotestsrc",NULL);
    g_object_set (G_OBJECT (zeroAudio), "volume", 0.0 , NULL);  
    
    gst_bin_add_many (GST_BIN (pipeline),
		      zeroAudio,
		      mixer,
		      audiosink,	   
		      NULL);

    gst_element_link_many (zeroAudio,
			   mixer,
			   audiosink,	   
			   NULL);
    
    /* message handler */
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    //    add_sample (pipeline,mixer);
    // request new sample periodically
    g_timeout_add (2000, (GSourceFunc) play_sample, NULL);

    /* Set the pipeline to "playing" state*/
    g_print ("Now playing\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* if (!gst_element_seek (pipeline, 0.6, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, */
  /*                        GST_SEEK_TYPE_SET, 0, */
  /*                        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))  */
  /*   { */
  /*     g_print ("Seek failed!\n"); */
  /*   } */

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
