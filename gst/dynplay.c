#include <gst/gst.h>
#include <signal.h>

static GstElement *pipeline;
static GstElement *audiomixer;
static GstElement *videomixer;
static GstElement *inputselector;

typedef enum GourpState_ {
  GROUP_TO_PLAYING = 0,
  GROUP_PLAYING = 1,
  GROUP_TO_PAUSED = 2,
  GROUP_PAUSED =3
} GroupState;


typedef struct
{
  GstElement *bin, *audiomixer, *videomixer,  *pipeline;
  GroupState state;
  GHashTable *datastreams; 
  GAsyncQueue *commands;
  GstPad *masterpad; //pad of the sample that determine end of play
  // a thread safe counter of task to do before actually going to a state
  // and unlocking new command
  GAsyncQueue *numTasks; 
  gboolean isseeking;
} Group;

typedef struct 
{
  gboolean (*func)(gpointer, gpointer);
  Group *group;
  gpointer arg;
} GroupCommand;

typedef struct
{
  //GstElement *audioconvert, *pitch, *resample;
  GstElement *seek_element;
  GstPad *bin_srcpad;  // ghost pad associated with the sample
  //GstPad *resample_srcpad;
  Group *group; // group to which the sample belongs
  GstClockTime timeshift; //shifting timestamps for video
  GstClockTime lastpause;
} Sample;

gboolean group_seek (Group *group);
gboolean group_pause (Group *group);
void group_do_group_seek (Group *group);
void group_unlink_datastream (gpointer key, gpointer value, gpointer user_data);
void pad_blocked_cb (GstPad *pad, gboolean blocked, gpointer user_data);
void group_block_datastream (gpointer key, gpointer value, gpointer user_data);
void group_seek_datastream (gpointer key, gpointer value, gpointer user_data);
void group_add_task (gpointer key, gpointer value, gpointer user_data);


void 
unlink_pad (GstPad *pad)
{
  if (!GST_IS_PAD (pad))
    {
      g_print ("warning (unlink_pad): trying to unlink something not a pad\n");
      return;
    }

  GstPad *peerpad = gst_pad_get_peer (pad);
  if (GST_IS_PAD (peerpad))
    {
      GstElement *peerpadparent = gst_pad_get_parent_element (peerpad);  
      g_print ("unlink returns %d\n",gst_pad_unlink (pad, peerpad));  
      // give back the pad     
      gst_element_release_request_pad (peerpadparent, peerpad);  
      gst_object_unref (peerpad);  
      gst_object_unref (peerpadparent);
    }
}


gboolean
group_eos_rewind (Group *group)
{
  g_print ("group_eos_rewind\n");
  group->state = GROUP_TO_PAUSED; 
  g_hash_table_foreach (group->datastreams,(GHFunc)group_add_task,group);
  g_hash_table_foreach (group->datastreams,(GHFunc)group_add_task,group);
  g_hash_table_foreach (group->datastreams,(GHFunc)group_block_datastream,NULL);
  g_hash_table_foreach (group->datastreams,(GHFunc)group_seek_datastream,NULL);
  group_play (group);	     
  return FALSE;
}



static gboolean
buffer_probe_cb (GstPad *pad, GstBuffer *buffer, gpointer data)
{
  //g_print("%llu \n", GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP (buffer)) );
  
  Sample *sample = (Sample *)data;

  if ( GST_BUFFER_TIMESTAMP (buffer) == 0)
    {
      g_print ("WINWIWNWIWNIWNIWNIWNIW\n");
      GstClock *clock = gst_pipeline_get_clock (GST_PIPELINE (sample->group->pipeline));
      sample->timeshift =  gst_clock_get_time (clock) - gst_element_get_start_time (sample->group->pipeline);
      g_print("%llu \n", GST_TIME_AS_MSECONDS(sample->timeshift) );
      gst_object_unref (clock);     
    }
  
  GST_BUFFER_TIMESTAMP (buffer) = GST_BUFFER_TIMESTAMP (buffer) + sample->timeshift;
  
  return TRUE;
}

static gboolean
event_probe_cb (GstPad *pad, GstEvent * event, gpointer data)
{
  Sample *sample = (Sample *)data;

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) { 
      //g_print ("EOS caught and disabled \n");
    g_print ("-------------------------------------------------pad with EOS %s:%s, pointer:%p src: %p %s\n",
	     GST_DEBUG_PAD_NAME (pad),pad,GST_EVENT_SRC(event), gst_element_get_name (GST_EVENT_SRC(event)));


    if (sample->group->state != GROUP_PLAYING)
      g_print ("OMGOMGOMGOMGOMGOMGOMGOMGOMGOMGOMGOMGOMGOMGOMG %p\n",&(sample->group->state));

    group_unlink_datastream ((gpointer)sample,NULL,NULL);   
    
    /* if (!gst_pad_is_blocked (pad)) */
    /*   if (!gst_pad_set_blocked_async (pad,TRUE,(GstPadBlockCallback)pad_blocked_cb,sample))  */
    /* 	g_print ("EOS: pb blocking\n");  */
  
    
    if (pad == sample->group->masterpad)  
      {  
	// this seems not working if play rate >1.0

	g_print ("!!! masterpad\n");  
	
	/* sample->group->state = GROUP_TO_PAUSED;  */
	/* g_hash_table_foreach (sample->group->datastreams,(GHFunc)group_add_task,sample->group); */
	/* g_hash_table_foreach (sample->group->datastreams,(GHFunc)group_add_task,sample->group); */
	/* g_hash_table_foreach (sample->group->datastreams,(GHFunc)group_block_datastream,NULL); */
	/* g_hash_table_foreach (sample->group->datastreams,(GHFunc)group_seek_datastream,NULL); */
	/* group_play (sample->group); */

	  g_idle_add ((GSourceFunc) group_eos_rewind,   
	  	    (gpointer)sample->group);   

	 
	 /* gst_pad_send_event (pad,        			         */
	 /* 		     gst_event_new_seek (1.0,            */
	 /* 					 GST_FORMAT_TIME,            */
	 /* 					 GST_SEEK_FLAG_FLUSH            */
	 /* 					 | GST_SEEK_FLAG_ACCURATE,            */
	 /* 					 GST_SEEK_TYPE_SET,            */
	 /* 					 0.0 * GST_SECOND,            */
	 /* 					 GST_SEEK_TYPE_NONE,            */
	 /* 					 GST_CLOCK_TIME_NONE)           */
	 /* 		     );           */
	 
     	
      } 
    
    /* gst_pad_send_event (pad,        			    */
    /* 			   gst_event_new_seek (1.0,       */
    /* 					       GST_FORMAT_TIME,       */
    /* 					       GST_SEEK_FLAG_FLUSH       */
    /* 					       | GST_SEEK_FLAG_ACCURATE,       */
    /* 					       GST_SEEK_TYPE_SET,       */
    /* 					       0.0 * GST_SECOND,       */
    /* 					       GST_SEEK_TYPE_NONE,       */
    /* 					       GST_CLOCK_TIME_NONE)      */
       /* 			   );      */
    
    
    
       //g_print ("return FALSE\n");
    return FALSE; 
  }
  
  //  g_print ("event received :%d\n",GST_EVENT_TYPE (event));
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
    g_print ("bus_call End of stream, name: %s\n",
	     GST_MESSAGE_SRC_NAME(msg));
    //g_main_loop_quit (loop);
    break;
  case GST_MESSAGE_SEGMENT_DONE:
    g_print ("bus_call segment done\n");
    break;
  case GST_MESSAGE_ERROR: {
    gchar *debug;
    GError *error;
	
    gst_message_parse_error (msg, &error, &debug);
    g_free (debug);
      
    g_printerr ("bus_call Error: %s\n", error->message);
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
  
  if (gst_pad_is_linked (sample->bin_srcpad))
    {
      g_print (".....................oups, already linked \n");
    }

  /* gboolean switchinputselector = FALSE; */

  GstPad *peerpad;
  peerpad = gst_element_get_compatible_pad (group->audiomixer,sample->bin_srcpad,NULL);
  //g_print("is pad: %d %d\n",GST_IS_PAD(sample->bin_srcpad), GST_IS_PAD (peerpad));
  if (!GST_IS_PAD (peerpad))
    {
      //trying video
       if (!GST_IS_PAD (sample->bin_srcpad)) 
       	{ 
       	  sample->bin_srcpad = gst_element_get_compatible_pad (group->videomixer,sample->bin_srcpad,NULL); 
       	} 
       peerpad = gst_element_get_compatible_pad (group->videomixer,sample->bin_srcpad,NULL);

    }
  
  if (GST_IS_PAD (peerpad))
    {
      g_print ("pad_link: %d \n",gst_pad_link (sample->bin_srcpad, peerpad));


      gst_object_unref (peerpad);
      if (!gst_pad_is_linked (sample->bin_srcpad))
	{
	  g_print (".....................oups, link did not worked \n");
	}
    }
  
}


void
group_unlink_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  
  unlink_pad (sample->bin_srcpad);

}

void
group_add_task (gpointer key, gpointer value, gpointer user_data)
{
  Group *group = (Group *) user_data;
  g_async_queue_push (group->numTasks,group);
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
  if (user_data != NULL)
    {
      (*command->func) (command->group,command->arg);
      if (command->arg != NULL)
	{
	  g_print ("freeing coommand arg\n");
	  g_free (command->arg);
	}
      g_free (command);
    }
  return FALSE; //to be removed from the gmain loop
}

void 
group_launch_queued_command (Group *group)
{

}

void 
group_try_change_state (gpointer user_data)
{
  Group *group = (Group *) user_data;

  g_print ("try to change state %d\n", g_async_queue_length (group->numTasks));

  if (g_async_queue_length (group->numTasks) != 0 )
    {
      if (NULL == g_async_queue_try_pop (group->numTasks))
	g_print ("warning: queue not poped.........................\n ");
      
      if (g_async_queue_length (group->numTasks) == 0 ) 
	{ 
	  if (group->state == GROUP_TO_PLAYING) 
	    {
	      group->state = GROUP_PLAYING;  
	      g_print ("back to playing state %p\n", &(group->state));
	    }
	  else if (group->state == GROUP_TO_PAUSED) 
	    {
	      group->state = GROUP_PAUSED;
	      g_print ("back to plaused state\n");
	    }

	  GroupCommand *command = g_async_queue_try_pop (group->commands);
	  //launching the command with the higher priority 
	  if (command != NULL)
	    g_idle_add_full (G_PRIORITY_DEFAULT,
			     &group_launch_command,
			     (gpointer)command,
			     NULL);
	  //g_print ("queue size %d\n",g_async_queue_length (group->numTasks));
	  //g_print ("** new command launched\n");
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
      //g_print ("pad blocked and unlinked\n");
      g_print ("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX pad_blocked_cb %p\n",pad);
      group_try_change_state (sample->group);
    }
  else
    g_print ("xxxxxxxxxxxxxxxxxxxxx pad unblocked\n");
    

}

void
group_unblock_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  Group *group = (Group *) user_data;
  
  g_print ("group_unblock_datastream\n");
 
  if (!gst_pad_is_blocked (sample->bin_srcpad) || !gst_pad_is_linked (sample->bin_srcpad))
    g_print ("ARGARGARGARTGAARATGARATAGRAGA - - trying to unblock a not blocked pad\n");
  
  if (!gst_pad_set_blocked_async (sample->bin_srcpad,FALSE,(GstPadBlockCallback)pad_blocked_cb,sample))
    {
      g_print  ("play_source: pb unblocking %p\n",sample->bin_srcpad);
      }
  group_try_change_state (sample->group);
    
}

void
group_block_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;

  if (!gst_pad_is_blocked (sample->bin_srcpad))
    {
      if (!gst_pad_set_blocked_async (sample->bin_srcpad,TRUE,(GstPadBlockCallback)pad_blocked_cb,sample))
	  g_print ("play_source: pb blocking\n");
    }
  else
    {
      g_print ("not blocking unblocked pad\n");
      group_try_change_state (sample->group);
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

void
group_queue_command_unlocked (Group *group, gpointer func, gpointer arg)
{
  GroupCommand *command = g_new0 (GroupCommand,1);
  command->func = func;
  command->group = group;
  command->arg = arg;
  g_async_queue_push_unlocked (group->commands,command);
  g_print ("-- queuing (unlocked) command\n");
}


gboolean
group_play_wrapped_for_commands (gpointer user_data, gpointer user_data2)
{
  Group *group = (Group *) user_data;
  return group_play (group);
}

gboolean    
group_play (Group *group){   

  g_print ("trying to play state %d, queue lenth %d\n",group->state, g_async_queue_length (group->numTasks));

  if (!GST_IS_ELEMENT(group->bin))   
    {   
      g_print ("trying to play something not a GStreamer element\n");   
      return FALSE;   
    }   
  
  switch ( group->state ) {
  case GROUP_PAUSED:
    g_print ("*********************** play group\n");   
    group->state = GROUP_TO_PLAYING;   
    g_hash_table_foreach (group->datastreams,(GHFunc)group_add_task,group);
    g_hash_table_foreach (group->datastreams,(GHFunc)group_link_datastream,group);
    g_hash_table_foreach (group->datastreams,(GHFunc)group_unblock_datastream,group);
    break;
  case GROUP_TO_PAUSED:
    group_queue_command (group, &group_play_wrapped_for_commands, NULL);
    break;
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
    g_print ("try to pause, state is %d\n",group->state);

  switch ( group->state ) {
  case GROUP_PAUSED:
    break;
  case GROUP_TO_PAUSED:
      //group_queue_command (group,&group_pause_wrapped_for_commands,NULL);
    break;
  case GROUP_PLAYING:
    g_print ("*********************** group pause\n");
    group->state = GROUP_TO_PAUSED;
    g_hash_table_foreach (group->datastreams,(GHFunc)group_add_task,group);
    g_print ("task added\n");
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
      
      GstElement *audioconvert = gst_element_factory_make ("audioconvert",NULL);   
      GstElement *pitch = gst_element_factory_make ("pitch",NULL);   
      GstElement *resample = gst_element_factory_make ("audioresample",NULL);   
      sample->seek_element = resample;
      g_object_set (G_OBJECT (resample), "quality", 10 , NULL);  
      g_object_set (G_OBJECT (resample), "filter-length", 2048 , NULL); 

      /* add to the bin */   
      gst_bin_add (GST_BIN (group->bin), audioconvert);   
      gst_bin_add (GST_BIN (group->bin), pitch);   
      gst_bin_add (GST_BIN (group->bin), resample);   

      if (!gst_element_sync_state_with_parent (audioconvert))      
	g_error ("pb syncing audio datastream state\n");      
      if (!gst_element_sync_state_with_parent (pitch))      
	g_error ("pb syncing audio datastream state\n");      
      if (!gst_element_sync_state_with_parent (resample))      
	g_error ("pb syncing audio datastream state\n");      
      
      GstPad *resample_srcpad = gst_element_get_static_pad (resample, "src");
      sample->bin_srcpad = gst_ghost_pad_new (NULL, resample_srcpad);
      gst_pad_set_blocked_async (sample->bin_srcpad,TRUE,pad_blocked_cb,sample);
      gst_object_unref (resample_srcpad);
      gst_pad_set_active(sample->bin_srcpad,TRUE);
      gst_element_add_pad (group->bin, sample->bin_srcpad);    
      if (group->masterpad == NULL)
	  {
	      g_print ("---------------- masterpad is %p\n",sample->bin_srcpad);
	      group->masterpad = sample->bin_srcpad;
	  }
      //probing eos   
      gst_pad_add_event_probe (sample->bin_srcpad, (GCallback) event_probe_cb, (gpointer)sample);   
      
      GstPad *audioconvert_sinkpad = gst_element_get_static_pad (audioconvert, "sink");
      
      //linking newly created pad with the audioconvert_sinkpad -- FIXME should verify compatibility     
      gst_pad_link (pad, audioconvert_sinkpad);   
      gst_object_unref (audioconvert_sinkpad);
      gst_element_link_many (audioconvert, pitch, resample,NULL);   

      //g_print ("%s\n",gst_object_get_name(GST_OBJECT_CAST(object)));
            
      //assuming object is an uridecodebin and get the uri 
       gchar *uri; 
       g_object_get (object,"uri",&uri,NULL); 
       g_print ("new sample -- uri: %s\n",uri); 
      //adding sample to hash table
       g_hash_table_insert (group->datastreams,sample,uri); //FIXME clean hash 
      
    }
  else if (g_str_has_prefix (padname, "video/"))
    {

      group_add_task (NULL,NULL,group);  
      Sample *sample = g_new0 (Sample,1);  
      sample->group = group;  
      sample->timeshift = 0;
      
      GstElement *queue = gst_element_factory_make ("queue",NULL);  
      sample->seek_element = queue;  
      
      /* add to the bin */     
      gst_bin_add (GST_BIN (group->bin), queue);     
      
      GstPad *queue_srcpad = gst_element_get_static_pad (queue, "src");   
      
      sample->bin_srcpad = gst_ghost_pad_new (NULL, queue_srcpad);     
      gst_pad_set_blocked_async (sample->bin_srcpad,TRUE,pad_blocked_cb,sample);     
      gst_object_unref (queue_srcpad);     
      gst_pad_set_active(sample->bin_srcpad,TRUE);     
      gst_element_add_pad (group->bin, sample->bin_srcpad);         
      if (group->masterpad == NULL)     
	{     
	  g_print ("---------------- masterpad is %p\n",sample->bin_srcpad);     
	  group->masterpad = sample->bin_srcpad;     
	}     
      
      //probing eos        
      gst_pad_add_event_probe (sample->bin_srcpad, (GCallback) event_probe_cb, (gpointer)sample);        
      /* gst_pad_add_buffer_probe (sample->bin_srcpad,   */
      /* 				(GCallback) buffer_probe_cb,   */
      /* 				(gpointer)sample);   */
      
      
      GstPad *queue_sinkpad = gst_element_get_static_pad (queue, "sink");  
      
      //linking newly created pad with the audioconvert_sinkpad -- FIXME should verify compatibility       
      gst_pad_link (pad, queue_sinkpad);     
      gst_object_unref (queue_sinkpad);  
      
      if (!gst_element_sync_state_with_parent (queue))        
       	g_error ("pb syncing video datastream state\n");      
      
      //assuming object is an uridecodebin and get the uri    
      gchar *uri;    
      g_object_get (object,"uri",&uri,NULL);    
      g_print ("new sample -- uri: %s\n",uri);    
      //adding sample to hash table   
      g_hash_table_insert (group->datastreams,sample,uri); //FIXME clean hash    
      
      group->videomixer = inputselector;  
      
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
    g_print ("group_add_uri: setting GROUP_TO_PAUSED\n ");
    group->state = GROUP_TO_PAUSED;
    group_add_task (NULL,NULL,group);
    break;
  case GROUP_TO_PAUSED:
    uri_tmp = g_strdup(uri);
    group_queue_command (group, group_add_uri, uri_tmp);
    return;
    break;
  case GROUP_PLAYING:
    g_print ("group_add_uri: setting GROUP_TO_PLAYING\n ");
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

  g_print ("*********************** adding uri\n");

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
   		"ring-buffer-max-size",(guint64)200000000, 
   		"download",TRUE, 
   		"use-buffering",TRUE, 
   		"async-handling",TRUE, 
   		"buffer-duration",9223372036854775807, 
   		NULL); 
  g_object_set (G_OBJECT (src), "uri",   
    		uri, NULL);  

  gst_bin_add (GST_BIN (group->bin), src);
  //gst_element_set_state (src, GST_STATE_PLAYING);
  if (!gst_element_sync_state_with_parent (src))  
    g_error ("add_uri: pb sync uridecodebin state with parent\n");  
} 


/* static void */
/* group_segment_done_cb (GstBus * bus, GstMessage * message, */
/*     gpointer user_data) */
/* { */
/*   Group *group = (Group *) user_data; */

/*   g_print ("segment done !!\n"); */
/* } */

static Group *   
group_create (GstElement *pipeline, GstElement *audiomixer)   
{   
  Group *group;   
  g_print ("create group\n");   
  
  group = g_new0 (Group, 1);   
  group->commands = g_async_queue_new ();
  group->numTasks = g_async_queue_new ();
  group->audiomixer = audiomixer;
  group->pipeline = pipeline;   
  group->bin = gst_element_factory_make ("bin", NULL);
  //g_object_set (G_OBJECT (group->bin), "async-handling", TRUE , NULL);  

  group->masterpad = NULL;

  group->datastreams = g_hash_table_new (g_direct_hash, g_direct_equal);
  //allows to add and remove to/from the pipeline without disposing   
  //unref done in the group_remove function   
  gst_object_ref (group->bin);   
  gst_bin_add (GST_BIN (group->pipeline), group->bin);   
  //gst_element_set_state (group->bin,GST_STATE_READY); 
  if (!gst_element_sync_state_with_parent (group->bin)) 
    g_error ("create_group: pb sync bin state with parent\n"); 

  g_print ("group create: GROUP_PAUSED\n");
  group->state = GROUP_PAUSED;    
  return group;   
}   

void
group_do_seek_datastream (Sample *sample)
{
  
  g_print ("--------------: going to seek for a sample\n");
  
  gboolean ret;
  ret = gst_element_seek (sample->seek_element, 
   			  1.0, 
			  GST_FORMAT_TIME, 
			  GST_SEEK_FLAG_FLUSH
			  //| GST_SEEK_FLAG_SKIP
			  | GST_SEEK_FLAG_KEY_UNIT, 
			  GST_SEEK_TYPE_SET, 
			  310.0 * GST_SECOND, 
			  GST_SEEK_TYPE_NONE, 
			  GST_CLOCK_TIME_NONE); 
  if (!ret)
    g_print ("seek not handled\n");

  /* //if the seek is performed with an unlinked pad, but not blocked,  */
  /* //data will flow entirely before being linked, so blocking here  */
  /* //in that case (call this function when blocked  */
  /* if (!gst_pad_is_blocked (sample->bin_srcpad)) */
  /*   { */
  /*     g_print ("going to seek a not blocked pad! and unlinked pad\n"); */
  /*   } */
  
  gst_element_get_state (sample->seek_element,  
    			 NULL,  
    			 NULL,  
    			 GST_CLOCK_TIME_NONE);  
  g_print ("--------------: seek done for a sample\n");
  

  group_try_change_state (sample->group);

}

void 
group_seek_datastream (gpointer key, gpointer value, gpointer user_data)
{
  Sample *sample = (Sample *) key;
  
  /* g_idle_add_full (G_PRIORITY_DEFAULT,     */
  /* 		   (GSourceFunc)  group_do_seek_datastream,  */
  /* 		   (gpointer) sample, */
  /* 		   NULL); */
  group_do_seek_datastream (sample);
  
  return;
}

gboolean
group_seek_wrapped_for_commands (gpointer user_data, gpointer user_data2)
{
  Group *group = (Group *) user_data;
  return group_seek (group);
}

void
group_do_group_seek (Group *group)
{
  g_print ("*********************** group_seek\n");
  group_add_task (NULL,NULL,group);
  g_hash_table_foreach (group->datastreams,(GHFunc)group_add_task,group);
  g_hash_table_foreach (group->datastreams,(GHFunc)group_seek_datastream,NULL);
  group_try_change_state (group);
}

gboolean
group_seek (Group *group)
{
  g_print ("trying to seek, state %d\n",group->state);

  switch ( group->state ) {
  case GROUP_TO_PAUSED:
    group_queue_command (group, &group_seek_wrapped_for_commands, NULL);
    return FALSE;
    break;
  case GROUP_TO_PLAYING:
    group_queue_command (group, &group_seek_wrapped_for_commands, NULL);
    return FALSE;
    break;
  case GROUP_PAUSED:
    g_print ("group_seek: GROUP_TO_PAUSED\n");
    group->state = GROUP_TO_PAUSED; //using PAUSED state for seeking
    break;
  case GROUP_PLAYING:
    g_async_queue_lock (group->commands);
    group_queue_command_unlocked (group, &group_pause_wrapped_for_commands, NULL);
    group_queue_command_unlocked (group, &group_seek_wrapped_for_commands, NULL);
    group_queue_command_unlocked (group, &group_play_wrapped_for_commands, NULL);
    g_async_queue_unlock (group->commands);
    group_launch_command (g_async_queue_try_pop (group->commands));
    return FALSE;
    break;
  default:
    g_print ("unhandled state when seeking\n");
    break;
  }

  group_do_group_seek (group);
  
  return FALSE;
}


gboolean
test_play_vid (gpointer truc)
{
  GstElement *playbin = gst_element_factory_make ("playbin",NULL);
  g_object_set (G_OBJECT (playbin), "uri",   
    		"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Screen2.webm", NULL);  
  
  gst_bin_add (GST_BIN (pipeline), playbin);
  //gst_element_set_state (src, GST_STATE_PLAYING);
  if (!gst_element_sync_state_with_parent (playbin))  
    g_error ("pb sync playbin state with parent\n");  
    
  return FALSE;
}


static gboolean
run_test ()
{
  g_print ("adding a new group\n");
  
   Group *group1 = group_create (pipeline,audiomixer); 
    group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Ref.ogg");     
    group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Reverb.ogg");     
    group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Rhythm.ogg");     
    group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Sax.ogg");     
    group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Trombone.ogg");     
    group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Trumpet.ogg");     
    group_add_uri (group1,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Screen2.webm");   

      /*  group_add_uri (group1,"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Rhythm.ogg");      */
      /*  group_add_uri (group1,"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Ref.ogg");      */
      /*  group_add_uri (group1,"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Reverb.ogg");      */
      /*  group_add_uri (group1,"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Sax.ogg");      */
      /*  group_add_uri (group1,"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Trombone.ogg");      */
      /*  group_add_uri (group1,"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Trumpet.ogg");      */

      /* group_add_uri (group1,"file:///var/www/samples/HomeVersion/AmongThePyramids/Bass/AmongThePyramids_BassPosition_Screen2.webm");     */

   /* Group *group2 = group_create (pipeline,audiomixer);       */
   /*  group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Ambience.ogg");        */
   /*  group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Harpsichord.ogg ");        */
   
   /*  group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Orchestra.ogg ");        */
   
   /*  group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition_Ref.ogg ");        */
   //  group_add_uri (group2,"http://suizen.cim.mcgill.ca/oohttpvod/HomeVersion/Movement1/Bassoon/Movement1_BassoonPosition.webm");      
   
   /* //g_timeout_add (1000, (GSourceFunc) group_play, group2);    */
    //      group_play (group2);   


   group_play (group1);

  group_seek (group1);
  
    //    g_timeout_add (15000, (GSourceFunc) test_play_vid, NULL);   
  
  //group_pause (group1);  

  //g_timeout_add (10000, (GSourceFunc) group_play, group1);   
 
  //g_timeout_add (15000, (GSourceFunc) group_seek, group1);

   /* g_timeout_add (8000, (GSourceFunc) group_pause, group1);   */
 
   /* g_timeout_add (10000, (GSourceFunc) group_play, group1);     */

   /* g_timeout_add (12000, (GSourceFunc) group_pause, group1);   */
 
   /* g_timeout_add (14000, (GSourceFunc) group_play, group1);     */

   /* g_timeout_add (15000, (GSourceFunc) group_seek, group1);  */
   
   /* g_timeout_add (19000, (GSourceFunc) group_pause, group1);   */
      
   /* g_timeout_add (25000, (GSourceFunc) group_play, group1);      */

  /* g_timeout_add (30000, (GSourceFunc) group_play, group1);   */

  //do not repeat
  return FALSE;
  //do repeat
  //return TRUE;
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
   GstElement *audiosink = gst_element_factory_make ("pulsesink", NULL); 
   //g_object_set (G_OBJECT (audiosink), "sync", FALSE , NULL);   
  

   audiomixer = gst_element_factory_make ("adder",NULL); 

   GstElement *identity = gst_element_factory_make ("identity",NULL); 
   //g_object_set (G_OBJECT (identity), "single-segment", TRUE , NULL);    
   //g_object_set (G_OBJECT (identity), "datarate", 4 * 44100 , NULL);    

   GstElement *zeroAudio = gst_element_factory_make ("audiotestsrc",NULL); 
   g_object_set (G_OBJECT (zeroAudio), "volume", 0.0 , NULL);   

   GstCaps *caps = gst_caps_new_simple ("audio/x-raw-float", 
					"rate", G_TYPE_INT, 48000,
					"channels", G_TYPE_INT, 2,
					"endianness", G_TYPE_INT, 1234,
					"width", G_TYPE_INT, 32,
					NULL); 
   GstElement *capsfilter = gst_element_factory_make ("capsfilter",NULL); 
   g_object_set (G_OBJECT (capsfilter), "caps", caps , NULL);   

    gst_bin_add_many (GST_BIN (pipeline), 
		      zeroAudio, 
		      capsfilter,
		      audiomixer, 
		      identity, 
		      audiosink,	    
		      NULL); 
    
   gst_element_link_many (zeroAudio, 
			  capsfilter,
			  audiomixer, 
			  identity, 
			  audiosink,	    
			  NULL); 
   
    
   
   GstElement *videotestsrc = gst_element_factory_make ("videotestsrc",NULL);     
   g_object_set (G_OBJECT(videotestsrc), "pattern",2,NULL);
   GstCaps *vcaps = gst_caps_new_simple ("video/x-raw-yuv",   
     					 "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),  
     					 "width", G_TYPE_INT, 1280,  
     					 "height", G_TYPE_INT, 720,  
     					 "framerate", GST_TYPE_FRACTION, 10000000, 333667,  
     					 "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,  
     					 "interlaced",G_TYPE_BOOLEAN,FALSE,  
     					 NULL);   
   GstElement *vcapsfilter = gst_element_factory_make ("capsfilter",NULL);   
   g_object_set (G_OBJECT (vcapsfilter), "caps", vcaps , NULL);     
   
   
   inputselector = gst_element_factory_make ("videomixer",NULL);   
   //g_object_set (G_OBJECT (inputselector), "sync-streams", TRUE , NULL);   
   

   GstElement *xvimagesink = gst_element_factory_make ("xvimagesink",NULL);   
   
   gst_bin_add_many (GST_BIN (pipeline), 
		      videotestsrc, 
		      vcapsfilter, 
		     inputselector,
		     xvimagesink,NULL);   
   
   gst_element_link_many ( videotestsrc, 
			   vcapsfilter, 
			  inputselector, 
			  xvimagesink,NULL);      
   

  /* message handler */
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline)); 
  gst_bus_add_watch (bus, bus_call, loop); 
  gst_object_unref (bus); 

    
  // delay request 
  g_timeout_add (1000, (GSourceFunc) run_test, NULL);

  //request now
  //run_test ();


  /* Set the pipeline to "playing" state*/
  g_print ("Now playing pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

   /* if (!gst_element_seek (pipeline, 0.6, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,  */
   /*                        GST_SEEK_TYPE_SET, 0,  */
   /*                        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))   */
   /*   {  */
   /*     g_print ("Seek failed!\n");  */
   /*   }  */


  /* Iterate */
  g_print ("Pipeline running...\n");
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));

  return 0;
}
