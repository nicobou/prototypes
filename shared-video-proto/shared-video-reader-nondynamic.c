#include <gst/gst.h>
#include <signal.h>

GstElement *pipeline;


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

	g_main_loop_quit (loop);
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

    /* Set the pipeline to "playing" state*/
    g_print ("Now reading: %s\n", argv[1]);
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
