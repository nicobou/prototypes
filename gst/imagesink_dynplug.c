#include <gst/gst.h>
#include <signal.h>

GstElement *pipeline;
GstElement *xvimagesink;

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
	g_print ("End of stream\n");
	g_main_loop_quit (loop);
	break;

    case GST_MESSAGE_ERROR: {
	gchar *debug;
	GError *error;
      
	gst_message_parse_error (msg, &error, &debug);
	g_free (debug);
      
	g_printerr ("Error: %s\n", error->message);
	g_error_free (error);
      
	g_main_loop_quit (loop);
	break;
    }
    default:
	//g_print ("unknown message type \n");
	break;
    }
    return TRUE;
}

gboolean 
add_source(void *unused)
{
  
  GstElement *vidsrc = gst_element_factory_make ("videotestsrc", NULL);
  gst_bin_add_many (GST_BIN (pipeline),
		    vidsrc,	   
		    NULL);
  if (!gst_element_sync_state_with_parent (vidsrc))      
    g_error ("pb syncing audio datastream state\n");      
  
  gst_element_link_many (vidsrc, 
			 xvimagesink,	    
			 NULL); 
  return FALSE;
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
    
    xvimagesink = gst_element_factory_make ("xvimagesink", NULL);
    gst_bin_add_many (GST_BIN (pipeline),
		      xvimagesink,	   
		      NULL);

    //this is working
    add_source (NULL);

    /* gst_element_link_many (fakesrc, */
    /* 			   fakesink,	    */
    /* 			   NULL); */
    
    /* message handler */
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* Set the pipeline to "playing" state*/
    g_print ("Now playing\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    //these are not working
    //add_source (NULL);
    //g_timeout_add (3000, (GSourceFunc) add_source, NULL);

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
