#include <gst/gst.h>
#include <signal.h>

static GstElement *pipeline;
static GstElement *mixer;
GRand *randomGen; //for playback rate

typedef enum GourpState_ {
  //  GROUP_TO_CREATED,
  //GROUP_CREATED,
  GROUP_TO_PLAYING,
  GROUP_PLAYING,
  GROUP_TO_PAUSED,
  GROUP_PAUSED
} GroupState;


typedef struct
{
  GstElement *bin, *mixer, *pipeline;
  gdouble speedRatio;
  GroupState state;
  GHashTable *datastreams; 
  GAsyncQueue *commands;
  // a thread safe counter of task to do before actually going to a state
  // and unlocking new command
  GAsyncQueue *num_tasks; 
} Group;

typedef struct 
{
  gboolean (*func)(gpointer, gpointer);
  Group *group;
  gpointer arg;
} GroupCommand;

typedef struct
{
  GstElement *audioconvert, *pitch, *resample;
  GstPad *bin_srcpad;  // ghost pad associated with the sample
  GstPad *resample_srcpad;
  Group *group; // group to which the sample belongs
} Sample;

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
  //g_print("is pad: %d %d\n",GST_IS_PAD(sample->bin_srcpad), GST_IS_PAD (peerpad));
  gst_pad_link (sample->bin_srcpad, peerpad);
  gst_object_unref (peerpad);
}

void
group_unlink_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  
  GstPad *peerpad = gst_pad_get_peer (sample->bin_srcpad);
  if (GST_IS_PAD (peerpad))
    {
      GstElement *peerpadparent = gst_pad_get_parent_element (peerpad);  
      // unlink from mixer     
      gst_pad_unlink (sample->bin_srcpad, peerpad);  
      // give back the pad     
      gst_element_release_request_pad (peerpadparent, peerpad);  
      gst_object_unref (peerpad);  
      gst_object_unref (peerpadparent);
    }
}

void
group_add_task (gpointer key, gpointer value, gpointer user_data)
{
  Group *group = (Group *) user_data;
  g_async_queue_push (group->num_tasks,group);
  //g_print ("-- task added\n");
}

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

gboolean
group_launch_command (gpointer user_data)
{
  GroupCommand *command = (GroupCommand *) user_data;
  (*command->func) (command->group,command->arg);
  g_free (command);
  return FALSE; //to be removed from the gmain loop
}

void 
group_try_change_state (gpointer user_data)
{
  Group *group = (Group *) user_data;

  //g_print ("** try to change state %d\n", g_async_queue_length (group->num_tasks));

  if (g_async_queue_length (group->num_tasks) != 0 )
    {
    g_async_queue_try_pop (group->num_tasks);

     if (g_async_queue_length (group->num_tasks) == 0 ) 
       { 
	 if (group->state == GROUP_TO_PLAYING) 
	   group->state = GROUP_PLAYING;  
	 else if (group->state == GROUP_TO_PAUSED) 
	   group->state = GROUP_PAUSED;

	 //checking if a command is waiting for being called
	 if (g_async_queue_length (group->commands) != 0)
	   {
	     GroupCommand *command = g_async_queue_try_pop (group->commands);
	     //launching the command with the higher priority 
	     g_idle_add_full (G_PRIORITY_DEFAULT,
			      &group_launch_command,
			      (gpointer)command,
			      NULL);
	     //g_print ("queue size %d\n",g_async_queue_length (group->num_tasks));
	     //g_print ("** new command launched\n");
	   }
       } 
    }  
  else 
    {
      g_print ("nothing to do for changing the state\n");
    }
}

void 
pad_blocked_cb (GstPad *pad, gboolean blocked, gpointer user_data)
{
  Sample *sample = (Sample *) user_data;

  if (blocked)
    {
      group_unlink_datastream ((gpointer)sample,NULL,NULL);
    }

  group_try_change_state (sample->group);

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
    
  if (!gst_pad_set_blocked_async (sample->bin_srcpad,TRUE,(GstPadBlockCallback)pad_blocked_cb,sample))
    {
      g_print ("play_source: pb blocking\n");
    }
}

void
group_queue_command (Group *group, gpointer func, gpointer arg)
{
  GroupCommand *command = g_new0 (GroupCommand,1);
  command->func = func;
  command->group = group;
  command->arg = arg;
  g_async_queue_push (group->commands,command);
  g_print ("-- queuing command\n");
}

gboolean
group_play_wrapped_for_commands (gpointer user_data, gpointer user_data2)
{
  Group *group = (Group *) user_data;
  return group_play (group);
}

gboolean    
group_play (Group *group){   

  if (!GST_IS_ELEMENT(group->bin))   
    {   
      g_print ("trying to play something not a GStreamer element\n");   
      return FALSE;   
    }   
  
  /* if (group->state == GROUP_CREATED)    */
  /*   {    */
  /*     /\* if (!gst_element_sync_state_with_parent (group->bin))         *\/ */
  /*     /\* 	g_error ("group_play problem sync with parent\n");         *\/ */
  /*     g_print("----hash table size %d\n",g_hash_table_size (group->datastreams)); */
  /*     group->state = GROUP_PAUSED; */
  /*   } */

  switch ( group->state ) {
  case GROUP_PAUSED:
    g_print ("** play group\n");   
    group->state = GROUP_TO_PLAYING;   
    g_hash_table_foreach (group->datastreams,(GHFunc)group_add_task,group);
    g_hash_table_foreach (group->datastreams,(GHFunc)group_link_datastream,group);
    g_hash_table_foreach (group->datastreams,(GHFunc)group_unblock_datastream,group);
    break;
  case GROUP_TO_PAUSED:
    group_queue_command (group, &group_play_wrapped_for_commands, NULL);
    break;
  /* case GROUP_CREATED: */
  /*   break; */
  case GROUP_PLAYING:
    break;
  case GROUP_TO_PLAYING:
    break;
  default:
    g_print ("unknown state when playing");
    break;
  }
  
  return FALSE;   
}   

gboolean
group_pause_wrapped_for_commands (gpointer user_data, gpointer user_data2)
{
  Group *group = (Group *) user_data;
  return group_pause (group);
}

gboolean    
group_pause (Group *group)   
{   
  g_print ("pause group called\n");  

  switch ( group->state ) {
  case GROUP_PAUSED:
    break;
  case GROUP_TO_PAUSED:
    break;
  /* case GROUP_CREATED: */
  /*   break; */
  case GROUP_PLAYING:
    g_print ("** group pause\n");
    group->state = GROUP_TO_PAUSED;
    g_hash_table_foreach (group->datastreams,(GHFunc)group_add_task,group);
    g_hash_table_foreach (group->datastreams,(GHFunc)group_block_datastream,NULL);
    break;
  case GROUP_TO_PLAYING:
    group_queue_command (group,&group_pause_wrapped_for_commands,NULL);
    break;
  default:
    g_print ("unknown state when playing");
    break;
  }
  return FALSE;
}

void 
uridecodebin_no_more_pads_cb (GstElement* object, gpointer user_data)   
{   
  Group *group = (Group *) user_data;   
  //g_print ("** no more pads\n");
  group_try_change_state (group);
}

void uridecodebin_drained_cb (GstElement* object, gpointer user_data)   
{   
  Group *group = (Group *) user_data;   
  g_print ("drained\n");
}

void uridecodebin_pad_added_cb (GstElement* object, GstPad* pad, gpointer user_data)   
{   
  Group *group = (Group *) user_data;   
  
  const gchar *padname= gst_structure_get_name (gst_caps_get_structure(gst_pad_get_caps (pad),0));
  
  if (g_str_has_prefix (padname, "audio/"))
    {
      //adding a task to wait for launching next command
      group_add_task (NULL,NULL,group);

      Sample *sample = g_new0 (Sample,1);
      sample->group = group;
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
      
      gst_pad_set_blocked_async (sample->bin_srcpad,TRUE,pad_blocked_cb,sample);
    }
  else
    {
      g_print ("not handled data type: %s\n",padname);
      GstElement *fake = gst_element_factory_make ("fakesink", NULL);
      gst_bin_add (GST_BIN (group->bin),fake);
      if (!gst_element_sync_state_with_parent (fake))      
	g_error ("pb syncing datastream state: %s\n",padname);
      GstPad *fakepad = gst_element_get_static_pad (fake,"sink");
      gst_pad_link (pad,fakepad);
      gst_object_unref (fakepad);
    }
  return;   
}   

void 
group_add_uri (Group* group, const char *uri) 
{ 
  gchar *uri_tmp;

  switch ( group->state ) {
  case GROUP_PAUSED:
    group->state = GROUP_TO_PAUSED;
    group_add_task (NULL,NULL,group);
    break;
  case GROUP_TO_PAUSED:
    uri_tmp = g_strdup(uri);
    group_queue_command (group, group_add_uri, uri_tmp);
    return;
    break;
  case GROUP_PLAYING:
    group->state = GROUP_TO_PLAYING;
    group_add_task (NULL,NULL,group);
    break;
  case GROUP_TO_PLAYING:
    uri_tmp = g_strdup(uri);
    group_queue_command (group, group_add_uri, uri_tmp);
    return;
    break;
  default:
    g_print ("unknown state when adding uri\n");
    break;
  }

  g_print ("** adding uri\n");

  GstElement *src = gst_element_factory_make ("uridecodebin",NULL);   
  g_signal_connect (G_OBJECT (src), 
		    "pad-added", 
		    (GCallback) uridecodebin_pad_added_cb , 
		    (gpointer) group);
  g_signal_connect (G_OBJECT (src),  
   		    "no-more-pads",  
   		    (GCallback) uridecodebin_no_more_pads_cb ,  
   		    (gpointer) group);      
  g_signal_connect (G_OBJECT (src),  
   		    "drained",  
   		    (GCallback) uridecodebin_drained_cb ,  
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
  gst_element_set_state (src, GST_STATE_PLAYING);

} 

static Group *   
group_create (GstElement *pipeline, GstElement *mixer)   
{   
  Group *group;   
  g_print ("create group\n");   
  
  group = g_new0 (Group, 1);   
  group->commands = g_async_queue_new ();
  group->num_tasks = g_async_queue_new ();
  group->speedRatio = 1.0;   
  group->mixer = mixer;   
  group->pipeline = pipeline;   
  group->bin = gst_element_factory_make ("bin", NULL);   
  group->datastreams = g_hash_table_new (g_direct_hash, g_direct_equal);
  //allows to add and remove to/from the pipeline without disposing   
  //unref done in the group_remove function   
  gst_object_ref (group->bin);   
  gst_bin_add (GST_BIN (group->pipeline), group->bin);   
  gst_element_set_state (group->bin,GST_STATE_READY); 

  /* if (!gst_element_sync_state_with_parent (group->bin))         */
  /* 	g_error ("group_play problem sync with parent\n");     */

  group->state = GROUP_PAUSED;    
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
			  10.0 * GST_SECOND, 
			  GST_SEEK_TYPE_NONE, 
			  GST_CLOCK_TIME_NONE); 
  if (!ret)
    g_print ("seek not handled\n");
  
  return;
}

gboolean
group_seek_wrapped_for_commands (gpointer user_data, gpointer user_data2)
{
  Group *group = (Group *) user_data;
  return group_seek (group);
}

gboolean
group_seek (Group *group)
{
    
  switch ( group->state ) {
  case GROUP_TO_PAUSED:
    group_queue_command (group, &group_seek_wrapped_for_commands, NULL);
    return FALSE;
    break;
  case GROUP_TO_PLAYING:
    group_queue_command (group, &group_seek_wrapped_for_commands, NULL);
    return FALSE;
    break;
  default:
    g_print ("unhandled state when seeking\n");
    break;
  }

  g_print ("** group_seek\n");
  
  group_add_task (NULL,NULL,group);
  g_hash_table_foreach (group->datastreams,(GHFunc)group_seek_datastream,NULL);
  group_try_change_state (group);
  
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
  group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Screen2.webm");

 /*   Group *group2 = group_create (pipeline,mixer);  */
 /*   group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Ambience.ogg");  */
 /* group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Harpsichord.ogg ");  */

 /* group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Orchestra.ogg ");  */

 /* group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Ref.ogg ");  */

 /* g_timeout_add (1000, (GSourceFunc) group_play, group2); */
 /* g_timeout_add (1500, (GSourceFunc) group_seek, group2); */
 
  group_play(group1);

  group_pause (group1);

  group_seek(group1);

  group_play(group1);
 /* g_timeout_add (2000, (GSourceFunc) group_play, group1);  */
 
 /* g_timeout_add (3000, (GSourceFunc) group_seek, group1);   */
 
 /* g_timeout_add (5000, (GSourceFunc) group_pause, group1);    */
 
 /* g_timeout_add (6000, (GSourceFunc) group_play, group1);   */
 
 /* g_timeout_add (30000, (GSourceFunc) group_remove, group1); */
 
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
  g_print ("Now playing pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* if (!gst_element_seek (pipeline, 0.6, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, */
  /*                        GST_SEEK_TYPE_SET, 0, */
  /*                        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))  */
  /*   { */
  /*     g_print ("Seek failed!\n"); */
  /*   } */

  /* Iterate */
  g_print ("Pipeline running...\n");
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));

  g_rand_free (randomGen);

  return 0;
}
