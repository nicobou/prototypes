#include <gst/gst.h>
#include <signal.h>

static GstElement *pipeline;
static GstElement *playbin2;
static GstElement *vdisplay;
static GstElement *adisplay;
static GstElement *piperec;

gboolean
doexit (gpointer data)
{
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  exit(0);
}


void
leave(int sig) {
  g_print ("Returned, stopping playback\n");
  
   /* gst_element_set_state (piperec, GST_STATE_PAUSED);  */
  /* gst_object_unref (GST_OBJECT (piperec)); */
  
  /* gst_element_unlink (gst_bin_get_by_name (GST_BIN(adisplay),"atee"),     */
  /* 		      gst_bin_get_by_name (GST_BIN(piperec),"ain"));    */
  /* gst_element_unlink (GST_ELEMENT(gst_bin_get_by_name (GST_BIN(vdisplay),"vtee")),     */
  /*    		    gst_bin_get_by_name (GST_BIN(piperec),"vin"));    */
 
  gst_element_send_event(pipeline,
			 gst_event_new_eos());


  /* gst_bin_remove (GST_BIN (pipeline),  */
  /* 		  piperec); */
  

  //  g_timeout_add (5000, (GSourceFunc) doexit, NULL);

}

void
rec(int sig) {
  g_print ("hehe coucou\n");

  piperec = gst_parse_bin_from_description ("mp4mux name=muxrec ! filesink name=filerec location=res.mp4 sync=false  queue name=vin ! deinterlace ! ffmpegcolorspace ! x264enc ! queue ! muxrec.  queue name=ain ! audioconvert ! lamemp3enc bitrate=320 ! queue ! muxrec.",
						     FALSE, //make ghost pads,
						     NULL);
  gst_bin_add_many (GST_BIN (pipeline), 
		    piperec,
		    NULL); 
  
    gst_element_link (gst_bin_get_by_name (GST_BIN(adisplay),"atee"),   
    		    gst_bin_get_by_name (GST_BIN(piperec),"ain"));  
    gst_element_link (GST_ELEMENT(gst_bin_get_by_name (GST_BIN(vdisplay),"vtee")),   
    		    gst_bin_get_by_name (GST_BIN(piperec),"vin"));  

  if (!gst_element_sync_state_with_parent (piperec))        
      g_error ("pb syncing rec\n");      

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
    g_main_loop_quit (loop);
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


int
main (int argc,
      char *argv[])
{
  (void) signal(SIGINT,leave);

  (void) signal(SIGTSTP,rec);

  /* Initialisation */
  gst_init (&argc, &argv);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline  = gst_pipeline_new (NULL);

  
  if (!pipeline) {
    g_printerr ("The pipeline could not be created. Exiting.\n");
    return -1;
  }
    

 
  vdisplay = gst_parse_bin_from_description ("queue ! tee name=vtee ! queue ! autovideosink",
							 TRUE, //make ghost pads,
							 NULL);
  
  adisplay = gst_parse_bin_from_description ("queue ! tee name=atee ! queue ! autoaudiosink",
							     TRUE, //make ghost pads,
							     NULL);
  

  playbin2 = gst_element_factory_make ("playbin2",NULL);  
  g_object_set (G_OBJECT (playbin2), 
		"uri",argv[1], 
		"video-sink", vdisplay,
		"audio-sink", adisplay,
	       NULL);
 
    gst_bin_add_many (GST_BIN (pipeline), 
		      playbin2,
		      NULL); 


   
  /* message handler */
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline)); 
  gst_bus_add_watch (bus, bus_call, loop); 
  gst_object_unref (bus); 

    
  /* Set the pipeline to "playing" state*/
  g_print ("Now playing pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);


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
