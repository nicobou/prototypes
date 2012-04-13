#include <gst/gst.h>
#include <signal.h>

static GstElement *pipeline;
static GstElement *mixer;
GRand *randomGen; //for playback rate

typedef enum GourpState_ {
  SOURCE_LOADED,
  SOURCE_PLAYING,
  SOURCE_PAUSED
} GroupState;

typedef struct
{
  GstElement *audioconvert, *pitch, *resample;
  // ghost pad associated with the sample
  GstPad *bin_srcpad;
  GstPad *resample_srcpad;
} Sample;


typedef struct
{
  GstElement *bin, *mixer, *pipeline;
  gdouble speedRatio;
  GroupState state;
  GHashTable *datastreams; 
} Group;


static gboolean
event_probe_cb (GstPad *pad, GstEvent * event, gpointer data)
{
  Group *group = (Group *)data;

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
    //g_idle_add((GSourceFunc)group_remove, group);
    //do not send EOS to mixer
    //return FALSE;
  }
  
  return TRUE;
}



void
leave(int sig) {
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_rand_free (randomGen);
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

void
group_link_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  Group *group = (Group *) user_data;

  GstPad *peerpad = gst_element_get_compatible_pad (group->mixer,sample->bin_srcpad,NULL);
  g_print("is pad: %d %d\n",GST_IS_PAD(sample->bin_srcpad), GST_IS_PAD (peerpad));
  g_print ("pad link: %d\n", gst_pad_link (sample->bin_srcpad, peerpad));
  gst_object_unref (peerpad);
}

/* void */
/* group_link_pad (GstPad *pad, Group* group) */
/* { */
/*   GstPad *peerpad = gst_element_get_compatible_pad (group->mixer,pad,NULL); */
/*   g_print("is pad: %d %d\n",GST_IS_PAD(pad), GST_IS_PAD (peerpad)); */
/*   g_print ("pad link: %d\n", gst_pad_link (pad, peerpad)); */
/*   gst_object_unref (peerpad); */
/* }    */

void
group_unlink_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;

  GstPad *peerpad = gst_pad_get_peer (sample->bin_srcpad);
  GstElement *peerpadparent = gst_pad_get_parent_element (peerpad);
  // unlink from mixer   
  g_print ("unlink: %d\n",gst_pad_unlink (sample->bin_srcpad, peerpad));
  // give back the pad   
  gst_element_release_request_pad (peerpadparent, peerpad);
  gst_object_unref (peerpad);
  gst_object_unref (peerpadparent);
}

/* void     */
/* group_unlink_pad (GstPad *pad)    */
/* {    */
/*   GstPad *peerpad = gst_pad_get_peer (pad); */
/*   GstElement *peerpadparent = gst_pad_get_parent_element (peerpad); */
/*   /\* unlink bin from mixer *\/    */
/*   g_print ("unlink: %d\n",gst_pad_unlink (pad, peerpad)); */
/*   /\* give back the pad *\/    */
/*   gst_element_release_request_pad (peerpadparent, peerpad); */
/*   gst_object_unref (peerpad); */
/*   gst_object_unref (peerpadparent); */
/* }    */

/* void */
/* group_link (Group *group) */
/* { */
/*   //linking bin with mixer */
/*   // get new pad from adder, adder will now wait for data on this pad */
/*   // assuming this is a request pad */

/*   g_hash_table_foreach (group->datastreams,(GHFunc)group_link_datastream,group); */

/*   /\* int i; *\/ */
/*   /\* for (i=0;i<6;i++) *\/ */
/*   /\*   { *\/ */
/*   /\*     group_link_pad (group->samples[i]->bin_srcpad,group); *\/ */
/*   /\*   } *\/ */
/* }    */

/* void     */
/* group_unlink(Group *group)    */
/* {    */
/*   g_hash_table_foreach (group->datastreams,(GHFunc)group_unlink_datastream,group); */
  
/*   /\* int i; *\/ */
/*   /\* for (i=0;i<6;i++) *\/ */
/*   /\*   { *\/ */
/*   /\*     group_unlink_pad (group->samples[i]->bin_srcpad); *\/ */
/*   /\*   } *\/ */
/* }    */


static gboolean   
autoplug_continue_cb (GstElement * uridecodebin, GstPad * somepad, GstCaps * caps, gpointer user_data)   
{   
  g_print ("autoplug_continue_cb \n");   
  return TRUE;   
}   



/* remove the source from the pipeline after removing it from adder */   
static gboolean   
group_remove (Group * group)   
{   

  if (GST_IS_ELEMENT (group->bin))   
    {   
      g_print ("remove source\n");   
       
      /* lock the state so that we can put it to NULL without the parent messing   
       * with our state */   
      gst_element_set_locked_state (group->bin, TRUE);   

      g_hash_table_foreach (group->datastreams,(GHFunc)group_unlink_datastream,group);
      
      /* first stop the source. Remember that this might block when in the PAUSED   
       * state. Alternatively one could send EOS to the source, install an event   
       * probe and schedule a state change/unlink/release from the mainthread. */   
      /* NOTE that the source inside the bin will emit EOS but it will not reach   
       * adder because the element after the source is shut down first. We will send   
       * EOS later */   
      gst_element_set_state (group->bin, GST_STATE_NULL);   
      
      /* remove from the bin */   
      gst_bin_remove (GST_BIN (pipeline), group->bin);   
      
      gst_object_unref (group->bin);   

      
      //FIXME call sample destuction
     
      g_free (group);   
    }   
  return FALSE;   
}   


void 
pad_blocked_cb (GstPad *pad, gboolean blocked, gpointer user_data)
{
  if (blocked)
    {
      Sample *sample = (Sample *) user_data;
      group_unlink_datastream ((gpointer)sample,NULL,NULL);
      //group_unlink_pad (pad);
    }
}

void
group_unblock_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  Group *group = (Group *) user_data;
  
  if (!gst_pad_set_blocked_async (sample->bin_srcpad,FALSE,(GstPadBlockCallback)pad_blocked_cb,sample))
    {
      g_print  ("play_source: pb unblocking\n");
    }
}
void

group_block_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  Group *group = (Group *) user_data;
  
  if (!gst_pad_set_blocked_async (sample->bin_srcpad,TRUE,(GstPadBlockCallback)pad_blocked_cb,group))
    {
      g_print  ("play_source: pb blocking\n");
    }
}

static gboolean    
group_play (Group *group){   

  if (!GST_IS_ELEMENT(group->bin))   
    {   
      g_print ("trying to play something not a GStreamer element\n");   
      return FALSE;   
    }   
  g_print ("play group\n");   

  //if the group has not been played before, linking is done into uridecodebin_pad_added_cb   
  //when the pad is discovered   
  if (group->state == SOURCE_PAUSED)   
    {   
      g_print ("group_play: SOURCE_PAUSED\n");
      
      g_hash_table_foreach (group->datastreams,(GHFunc)group_link_datastream,group);

      g_hash_table_foreach (group->datastreams,(GHFunc)group_unblock_datastream,group);

      group->state = SOURCE_PLAYING;   
    }   

  if (group->state == SOURCE_LOADED)   
    {   
      g_print ("group_play: SOURCE_LOADED\n");

      gst_bin_add (GST_BIN (group->pipeline), group->bin);    
      
      //setting state in order to link the ghostpad when created
      group->state = SOURCE_PLAYING;   
      
      if (!gst_element_sync_state_with_parent (group->bin))      
	g_error ("group_play problem sync with parent\n");      
      
      g_print("----------------------------------hash table size %d\n",g_hash_table_size (group->datastreams));
      g_hash_table_foreach (group->datastreams,(GHFunc)group_link_datastream,group);

    }
  
  g_print ("end of play\n");
  return FALSE;   
}   


static gboolean    
group_pause (Group *group)   
{   
  g_print ("pause group\n");   

  if (group->state == SOURCE_PLAYING){

    g_hash_table_foreach (group->datastreams,(GHFunc)group_unblock_datastream,group);
    
    /* int i; */
    /* for (i=0;i<6;i++) */
    /*   { */
    /* 	gst_pad_set_blocked_async (group->samples[i]->bin_srcpad,TRUE,(GstPadBlockCallback)pad_blocked_cb,group); */
    /*   } */
    group->state = SOURCE_PAUSED;
  }
  else
    {
      g_print ("warning about pause not handled in this state\n");
    }
  return FALSE;
}

void uridecodebin_pad_added_cb (GstElement* object, GstPad* pad, gpointer user_data)   
{   
  Group *group = (Group *) user_data;   
  
  const gchar *padname= gst_structure_get_name (gst_caps_get_structure(gst_pad_get_caps (pad),0));
  
  if (g_str_has_prefix (padname, "audio/"))
    {
      Sample *sample = g_new0 (Sample,1);

      //probing eos for cleaning   
      gst_pad_add_event_probe (pad, (GCallback) event_probe_cb, (gpointer)sample);   
      
      sample->audioconvert = gst_element_factory_make ("audioconvert",NULL);   
      sample->pitch = gst_element_factory_make ("pitch",NULL);   
      sample->resample = gst_element_factory_make ("audioresample",NULL);   

    
      /* add to the bin */   
      gst_bin_add (GST_BIN (group->bin), sample->audioconvert);   
      gst_bin_add (GST_BIN (group->bin), sample->pitch);   
      gst_bin_add (GST_BIN (group->bin), sample->resample);   

      if (!gst_element_sync_state_with_parent (sample->audioconvert))      
	g_error ("pb syncing audio datastream state\n");      
      if (!gst_element_sync_state_with_parent (sample->pitch))      
	g_error ("pb syncing audio datastream state\n");      
      if (!gst_element_sync_state_with_parent (sample->resample))      
	g_error ("pb syncing audio datastream state\n");      
      
      sample->resample_srcpad = gst_element_get_static_pad (sample->resample, "src");
      sample->bin_srcpad = gst_ghost_pad_new (NULL, sample->resample_srcpad); 
      gst_pad_set_active(sample->bin_srcpad,TRUE);
      gst_element_add_pad (group->bin, sample->bin_srcpad);    
      
      GstPad *audioconvert_sinkpad = gst_element_get_static_pad (sample->audioconvert, "sink");
      
      //linking newly created pad with the audioconvert_sinkpad -- FIXME should verify compatibility     
      gst_pad_link (pad, audioconvert_sinkpad);   
      gst_object_unref (audioconvert_sinkpad);
      gst_element_link_many (sample->audioconvert, sample->pitch, sample->resample,NULL);   

      //g_print ("%s\n",gst_object_get_name(GST_OBJECT_CAST(object)));

      //assuming object is an uridecodebin and get the uri 
       gchar *uri; 
       g_object_get (object,"uri",&uri,NULL); 
       g_print ("new sample commint with uri:%s\n",uri); 
      //adding sample to hash table
      g_hash_table_insert (group->datastreams,sample,uri); //FIXME clean uri when remove
      
      
    }
  else 
    {
      GstElement *fake = gst_element_factory_make ("fakesink", NULL);
      g_print ("unknown data type: %s\n",padname);
      GstPad *fakepad = gst_element_get_static_pad (fake,"sink");
      gst_pad_link (pad,fakepad);
      gst_object_unref (fakepad);
    }
  return;   
}   

void 
group_add_uri (Group* group, const char *uri) 
{ 
  g_print ("adding uri\n");

  GstElement *src = gst_element_factory_make ("uridecodebin",NULL);   
  g_signal_connect (G_OBJECT (src), 
		    "pad-added", 
		    (GCallback) uridecodebin_pad_added_cb , 
		    (gpointer) group);     
  g_object_set (G_OBJECT (src), 
		"ring-buffer-max-size",(guint64)20000000,
		"download",TRUE,
		"use-buffering",TRUE,
		"async-handling",TRUE,
		"buffer-duration",372036854775807,
		NULL);
  g_object_set (G_OBJECT (src), "uri",   
    		uri, NULL);  
  
  gst_bin_add (GST_BIN (group->bin), src);
  //FIXME sync state with parent
} 

static Group *   
group_create (GstElement *pipeline, GstElement *mixer)   
{   
  Group *group;   
  g_print ("create group\n");   

  group = g_new0 (Group, 1);   
  group->speedRatio = 1.0;   
  group->mixer = mixer;   
  group->pipeline = pipeline;   
  
  group->bin = gst_element_factory_make ("bin", NULL);   

  group->datastreams = g_hash_table_new (g_direct_hash, g_direct_equal);

  //allows to add and remove to/from the pipeline without disposing   
  //unref done in the group_remove function   
  gst_object_ref (group->bin);   

  //setting group properties    
    /* g_object_set (G_OBJECT (group->samples[1]->src), "uri",    */
    /* 		"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Ref.ogg", NULL);   */
    /* g_object_set (G_OBJECT (group->samples[2]->src), "uri",    */
    /* 		"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Reverb.ogg", NULL);   */
    /* g_object_set (G_OBJECT (group->samples[3]->src), "uri",    */
    /* 		"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Rhythm.ogg", NULL);   */
    /* g_object_set (G_OBJECT (group->samples[4]->src), "uri",    */
    /* 		"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Sax.ogg", NULL);   */
    /* g_object_set (G_OBJECT (group->samples[5]->src), "uri",    */
    /* 		"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Trombone.ogg", NULL);   */

  //g_object_set (G_OBJECT (group->src), "uri", "file:///usr/share/sounds/alsa/Front_Center.wav" , NULL);     
  //g_object_set (G_OBJECT (group->src), "uri", "http://localhost/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_AllNoRef.wav" , NULL);     
  //g_object_set (G_OBJECT (group->src), "download", TRUE , NULL);     
  //g_object_set (G_OBJECT (group->src), "uri", "http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_AllNoRef.mp3" , NULL);     
  /* g_object_set (G_OBJECT (group->src), "async-handling", TRUE , NULL);      */
  /* g_object_set (G_OBJECT (group->src), "message-forward", TRUE , NULL);      */
  gst_element_set_state (group->bin,GST_STATE_READY);   
  group->state = SOURCE_LOADED;   


  return group;   
}   

void 
group_seek_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  
  gboolean ret;
  ret = gst_element_seek (sample->resample, 
   			  1.5, 
			  GST_FORMAT_TIME, 
			  GST_SEEK_FLAG_FLUSH 
			  | GST_SEEK_FLAG_ACCURATE, 
			  GST_SEEK_TYPE_SET, 
			  100.0 * GST_SECOND, 
			  GST_SEEK_TYPE_NONE, 
			  GST_CLOCK_TIME_NONE); 
  if (!ret)
    g_print ("seek not handled\n");
  
  return;
}

static gboolean
seek_group (Group *group)
{
  g_print ("seek_group\n");
  
  GroupState state = group->state; 

  group_pause (group);
  
  g_hash_table_foreach (group->datastreams,(GHFunc)group_seek_datastream,NULL);
  /* int i; */
  /* for (i=0;i<6;i++)    */
  /*   {    */
  /*     seek_group_sample (group->samples[i]);    */
  /*   }    */
  
  g_usleep (G_USEC_PER_SEC);

  group_play (group);

  //  group_play (group);

  return FALSE;
}


static gboolean
run_test ()
{
  g_print ("adding a new group\n");

  Group *group1 = group_create (pipeline,mixer);
  group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Ref.ogg");
  group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Reverb.ogg");
  group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Rhythm.ogg");
  group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Sax.ogg");
  group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Trombone.ogg");
  group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Trumpet.ogg");
  //group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Screen2.webm");

  //  Group *group2 = group_create (pipeline,mixer);
  

  g_timeout_add (2000, (GSourceFunc) group_play, group1);

  g_timeout_add (4000, (GSourceFunc) seek_group, group1);

  //g_timeout_add (8000, (GSourceFunc) group_play, group2);

  g_timeout_add (16000, (GSourceFunc) group_pause, group1);
  //g_timeout_add (5000, (GSourceFunc) seek_group, group1);


  /* //speedRatio taken into account when seeking */
  /* group1->speedRatio = (gdouble) 2.0; */

  /* g_timeout_add (8000, (GSourceFunc) seek_group, group1); */

  /* g_timeout_add (10000, (GSourceFunc) group_pause, group1); */

  /* g_timeout_add (12000, (GSourceFunc) seek_group, group1); */

  /* g_timeout_add (14000, (GSourceFunc) group_play, group1); */

  g_timeout_add (30000, (GSourceFunc) group_remove, group1);

  return TRUE;
}


int
main (int argc,
      char *argv[])
{
  (void) signal(SIGINT,leave);
    
  randomGen = g_rand_new ();

    
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
  GstElement *audiosink = gst_element_factory_make ("pulsesink", NULL);
  //g_object_set (G_OBJECT (audiosink), "sync", FALSE , NULL);  
  
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

    
  // request periodically
  //g_timeout_add (14000, (GSourceFunc) run_test, NULL);
    
  //request once
  run_test ();

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

  g_rand_free (randomGen);

  return 0;
}
