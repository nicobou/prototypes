#include <gst/gst.h>
#include <signal.h>
#include <gio/gio.h>


GstElement *pipeline;
GFile *shmfile; 
GFileMonitor* dirMonitor;

static void
file_system_monitor_change (GFileMonitor *      monitor,
			    GFile *             file,
			    GFile *             other_file,
			    GFileMonitorEvent   type,
			    gpointer userdata)
{

    char *filename = g_file_get_path (file);
    
    switch (type)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
	if (g_file_equal (file,shmfile)) {
	    g_print ("Now reading: \n");
	    gst_element_set_state (pipeline, GST_STATE_PLAYING);
	}	  
	g_print ("G_FILE_MONITOR_EVENT_CREATED: %s\n",filename);
	break;
    case G_FILE_MONITOR_EVENT_DELETED:
	if (g_file_equal (file,shmfile)) {
	    g_print ("Now nulling: \n");
	    gst_element_set_state (pipeline, GST_STATE_NULL);
	}	  
	    g_print ("G_FILE_MONITOR_EVENT_DELETED: %s\n",filename);
	break;
	/* case G_FILE_MONITOR_EVENT_CHANGED: */
	/* 	  g_print ("G_FILE_MONITOR_EVENT_CHANGED\n"); */
	/* 	  break; */
	/* case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: */
	/* 	  g_print ("G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED\n"); */
	/* 	  break; */
	/* case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: */
	/* 	  g_print ("G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT\n"); */
	/* 	  break; */
	/* case G_FILE_MONITOR_EVENT_PRE_UNMOUNT: */
	/* 	  g_print ("G_FILE_MONITOR_EVENT_PRE_UNMOUNT\n"); */
	/* 	  break; */
	/* case G_FILE_MONITOR_EVENT_UNMOUNTED: */
	/* 	  g_print ("G_FILE_MONITOR_EVENT_UNMOUNTED\n"); */
	/* 	  break; */
    default:
        break;
    }
  
    g_free (filename);

}




static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
    GMainLoop *loop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
	g_print ("End of stream\n");
	g_main_loop_quit (loop);
	break;

    case GST_MESSAGE_ERROR: {
	gchar  *debug;
	GError *error;

	gst_message_parse_error (msg, &error, &debug);
	g_free (debug);

	g_printerr ("Error nico: %s\n", error->message);
	g_error_free (error);

	g_print ("Now nulling: \n");
	gst_element_set_state (pipeline, GST_STATE_NULL);
//	g_main_loop_quit (loop);
	break;
    }
    default:
	break;
    }

    return TRUE;
}

void
leave(int sig) {
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));
    g_object_unref(shmfile);
    g_object_unref(dirMonitor);
    exit(sig);
}


int
main (int   argc,
      char *argv[])
{

    (void) signal(SIGINT,leave);

    GMainLoop *loop;
    GstElement *source, *deserializer, *wdisplay;
    GstBus *bus;

    if (argc != 2) {
	g_printerr ("Usage: %s <socket-path>\n", argv[0]);
	return -1;
    }



    /* Initialisation */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    /* Create gstreamer elements */
    pipeline   = gst_pipeline_new ("shared-video-reader");
    source     = gst_element_factory_make ("shmsrc",  "video-source");
    deserializer = gst_element_factory_make ("gdpdepay",  "deserializer");
    wdisplay = gst_element_factory_make ("xvimagesink", "window-display");

    if (!pipeline || !source || !deserializer || !wdisplay) {
	g_printerr ("One element could not be created. Exiting.\n");
	return -1;
    }

    /* Set up the pipeline */
    g_object_set (G_OBJECT (source), "socket-path", argv[1], NULL);

    /* we add a message handler */
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* we add all elements into the pipeline */
    gst_bin_add_many (GST_BIN (pipeline),
		      source, deserializer, wdisplay, NULL);
 
    /* we link the elements together */
    gst_element_link_many (source, deserializer,wdisplay, NULL);

    
    //monitoring the shared memory file
    shmfile = g_file_new_for_commandline_arg (argv[1]);
    if (shmfile == NULL) {
	g_printerr ("argument not valid. Exiting.\n");
	return -1;
    }
    if (g_file_query_exists (shmfile,NULL)){
	g_print ("Now reading: \n");
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
    }
    
    GFile *dir = g_file_get_parent (shmfile);
    dirMonitor = g_file_monitor_directory (dir,  
     							 G_FILE_MONITOR_NONE, 
     							 NULL,/*GCancellable*/ 
     							 NULL/*GError*/); 
    g_object_unref(dir);
    g_signal_connect (dirMonitor, "changed", G_CALLBACK (file_system_monitor_change), NULL); 
    


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
