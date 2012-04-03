#include <gst/gst.h>
#include <signal.h>

GstElement *pipeline;
GstElement *mixer;
GRand *randomGen; //for frequencies

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

void add_sin (GstElement *pipeline,GstElement *mixer, GRand *randomGen)
{

    GstElement *sin;
    sin       = gst_element_factory_make ("audiotestsrc",NULL);
    g_object_set (G_OBJECT (sin), "volume", 0.01, NULL);  
    g_object_set (G_OBJECT (sin), "freq", g_rand_double_range (randomGen,400,500) , NULL);  
    

    if (!sin)  {
	g_printerr ("audiotestsrc could not be created. Exiting.\n");
    }
    else
	{
	    gst_bin_add_many (GST_BIN (pipeline),
			      sin,
			      NULL);
	    gst_element_link_many (sin,
				   mixer,
				   NULL);

	    if (!gst_element_sync_state_with_parent (sin))
		g_error ("audiotestsrc problem sync with parent\n");
	}

}

static gboolean
play_sin ()
{
    g_print ("adding a new sin\n");
    add_sin (pipeline,mixer,randomGen);
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

    add_sin (pipeline,mixer,randomGen);
    // request new sin periodically
    g_timeout_add (700, (GSourceFunc) play_sin, NULL);

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

    g_rand_free (randomGen);
	
    return 0;
}
