#include <gst/gst.h>
#include <signal.h>
#include <glib.h>
#include <glib/gprintf.h>

GstElement *pipeline;
GstElement *source, *deserializer, *inputSelector, *wdisplay;
GstElement *decodebin;
GstElement *videoReplacement;

GstPad *vidInputPad;
GstPad *replacementInputPad;




static gboolean  
switch_timer ()  
{  

    g_message ("switching");  
    //g_object_set (G_OBJECT (inputSelector), "active-pad",replacementInputPad , NULL);  
    g_object_set (G_OBJECT (inputSelector), "active-pad",vidInputPad , NULL);  

    return TRUE;  
}  



static void
cb_newpad (GstElement *decodebin,
	   GstPad     *pad,
	   gboolean    last,
	   gpointer    data)
{
    GstCaps *caps;
    GstStructure *str;
    GstPad *wdisplaypad;

    /* only link once */
    wdisplaypad = gst_element_get_static_pad (wdisplay, "sink");
    if (GST_PAD_IS_LINKED (wdisplaypad)) {
	g_object_unref (wdisplaypad);
	return;
    }



    /* check media type */
    caps = gst_pad_get_caps (pad);
    str = gst_caps_get_structure (caps, 0);
    if (!g_strrstr (gst_structure_get_name (str), "video")) {
	gst_caps_unref (caps);
	gst_object_unref (wdisplaypad);
	g_print ("not receiving video data \n");
	return;
    }

     
    /* link'n'play */
    gst_pad_link (pad, wdisplaypad);

    g_object_unref (wdisplaypad);
  
    g_print ("fin video replacement \n");

    gst_caps_unref (caps);
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

	g_object_set (G_OBJECT (inputSelector), "active-pad",replacementInputPad , NULL);

	if(0){
	    //get caps from deserializer
	    /* GstPad *deserialsrcPad = gst_element_get_pad (deserializer, "src"); */
	    /* GstCaps *vidcaps = gst_pad_get_caps (deserialsrcPad); */
	    
	    /* g_print (gst_caps_to_string (vidcaps)); */
   
	    /* g_print ("constructing vid replacement\n"); */
	    /* //will create an other input and switch to it */
	    /* videoReplacement = gst_element_factory_make ("videotestsrc",  NULL); */
	    /* GstState current; */
	    /* gst_element_get_state (wdisplay,&current,NULL,GST_CLOCK_TIME_NONE); */
	    /* if(gst_element_set_state (videoReplacement, current) != GST_STATE_CHANGE_SUCCESS)  */
	    /* 	g_printerr ("Error: issue changing video replacement state\n");  */
	    /* else{  */
	    /* 	gst_bin_add (GST_BIN (pipeline),videoReplacement); */
	    /* 	gst_element_link_filtered (videoReplacement,inputSelector,vidcaps); */
	    /* } */
	    
	    /* GstPad *vrPad = gst_element_get_pad (videoReplacement, "src");; */
	    /* GstPad *isPad = gst_pad_get_peer (vrPad); */

	    /* //switch from serializer to videotestsrc */
	    /* g_object_set (G_OBJECT (inputSelector), "active-pad", isPad, NULL); */
	    
	    /* /\* gst_object_unref (deserialsrcPad); *\/ */
	    /* /\* gst_object_unref (vidcaps); *\/ */
	    /* /\* gst_object_unref (vrPad); *\/ */
	    /* /\* gst_object_unref (isPad); *\/ */
	    /* g_print ("fin video replacement \n"); */
	}
	
	//g_main_loop_quit (loop);
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
    GstBus *bus;

    if (argc != 2) {
	g_printerr ("Usage: %s <socket-path>\n", argv[0]);
	return -1;
    }

    /* Initialisation */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    /* Create gstreamer elements */
    pipeline      = gst_pipeline_new ("shared-video-reader");

    videoReplacement = gst_element_factory_make ("videotestsrc",  NULL);

    source        = gst_element_factory_make ("shmsrc",  "video-source");
    deserializer  = gst_element_factory_make ("gdpdepay",  "deserializer");
    decodebin     = gst_element_factory_make ("decodebin2",  NULL);
    g_signal_connect (decodebin, "new-decoded-pad", G_CALLBACK (cb_newpad), NULL);
    wdisplay      = gst_element_factory_make ("xvimagesink", "window-display");
    inputSelector = gst_element_factory_make ("input-selector", NULL);
    

    if (!pipeline || !source || !deserializer || !wdisplay || !inputSelector || !decodebin /*|| !videoReplacement*/) {
	g_printerr ("One element could not be created. Exiting.\n");
	return -1;
    }

    /* Set up the pipeline */
    g_object_set (G_OBJECT (source), "socket-path", argv[1], NULL);
    g_object_set (G_OBJECT (wdisplay), "sync", FALSE, NULL);

    /* we add a message handler */
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* we add all elements into the pipeline */
    gst_bin_add_many (GST_BIN (pipeline),
		      videoReplacement,
		      source, deserializer, inputSelector, decodebin, wdisplay,NULL);
 
    /* we link the elements together */
    gst_element_link (videoReplacement,inputSelector);
    
    gst_element_link_many (source, deserializer, inputSelector, decodebin, NULL);
    
    GstPad *dserialPad = gst_element_get_pad (deserializer, "src"); 
    vidInputPad = gst_pad_get_peer (dserialPad);
    gst_object_unref (dserialPad);

    GstPad *vrPad = gst_element_get_pad (videoReplacement, "src");
    replacementInputPad = gst_pad_get_peer (vrPad); 
    gst_object_unref (vrPad); 


    /* Set the pipeline to "playing" state*/
    g_print ("Now reading: %s\n", argv[1]);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    //g_object_set (G_OBJECT (inputSelector), "active-pad",replacementInputPad , NULL);  
    g_object_set (G_OBJECT (inputSelector), "active-pad",vidInputPad , NULL);

    g_timeout_add (200, (GSourceFunc) switch_timer, NULL);

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
