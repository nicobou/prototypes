#include <gst/gst.h>
#include <signal.h>

GstElement *pipeline;

GstElement *source;    
GstElement *tee;       
GstElement *qserial;
GstElement *qlocalxv;  
GstElement *imgsink;   
GstElement *serializer;
GstElement *shmsink;   
GstElement *timeoverlay;
GstElement *camsource;

gboolean timereset=FALSE;
GstClockTime timeshift=0;

static gboolean
reset_time (GstPad * pad, GstMiniObject * mini_obj,
    gpointer user_data)
{
    if (GST_IS_EVENT (mini_obj)) {
//	GstEvent *ev = GST_EVENT_CAST (mini_obj);
//	g_print("----   event\n");
    }
    else if (GST_IS_BUFFER (mini_obj)) {
	GstBuffer *buffer = GST_BUFFER_CAST (mini_obj);
	if(timereset)
	{
	    timeshift=GST_BUFFER_TIMESTAMP(buffer);
	    timereset=FALSE;
	}
	GST_BUFFER_TIMESTAMP(buffer)=GST_BUFFER_TIMESTAMP(buffer) - timeshift;
    } 
    else if (GST_IS_MESSAGE (mini_obj)) {
//	g_print("----   message\n");
    }
    
    return TRUE;
    
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

	g_printerr ("Error: %s\n", error->message);
	g_error_free (error);

	g_main_loop_quit (loop);
	break;
    }
    default:
	break;
    }

    return TRUE;
}

static void
echo_pad_unblocked (GstPad * pad, gboolean blocked, gpointer user_data)
{
    if(blocked)
	g_printerr ("Error: pad not unblocked\n");
    else
    {
	timereset=TRUE;
    }
}



static void 
switch_to_new_serializer (GstPad * pad, gboolean blocked, gpointer user_data )
{

    //unlink the old serializer
    GstPad *srcPad=gst_element_get_static_pad(serializer,"src");
    GstPad *srcPadPeer=gst_pad_get_peer(srcPad);
    if (!gst_pad_unlink (srcPad,srcPadPeer))
	g_printerr("Error: cannot unlink src\n");

    GstPad *sinkPad=gst_element_get_static_pad(serializer,"sink");
    GstPad *sinkPadPeer=gst_pad_get_peer(sinkPad);
    if (!gst_pad_unlink (sinkPadPeer,sinkPad))
	g_printerr("Error: cannot unlink sink\n");

    gst_object_unref (srcPad);
    gst_object_unref (sinkPad);
    
    GstBin *bin=GST_BIN (GST_ELEMENT_PARENT(serializer));
	

    //supposed to be PLAYING, possible issue because of not taking care of pending state 
    GstState current;
    gst_element_get_state (serializer,&current,NULL,GST_CLOCK_TIME_NONE);
    //g_print("serializer state: %d \n",current);

    //get rid of the old serializer TODO ensure object has been cleaned up
    gst_element_set_state (serializer,GST_STATE_NULL);

    //waiting for possible async state change
    gst_element_get_state (serializer,NULL,NULL,GST_CLOCK_TIME_NONE);

    //creating and linking the new serializer
    serializer = gst_element_factory_make ("gdppay",  NULL); 
    if(gst_element_set_state (serializer, current) != GST_STATE_CHANGE_SUCCESS) 
     	g_printerr ("Error: issue changing newSerializer state\n"); 
    else{ 
     	//g_print ("changing serializer state\n"); 
     	gst_bin_add (bin,serializer); 
     	GstPad *newSinkPad=gst_element_get_static_pad (serializer,"sink"); 
     	GstPad *newSrcPad=gst_element_get_static_pad (serializer,"src"); 
     	gst_pad_link (newSrcPad,srcPadPeer); 
     	gst_pad_link (sinkPadPeer,newSinkPad); 
     	gst_object_unref (newSinkPad); 
     	gst_object_unref (newSrcPad); 
    }	 
    gst_object_unref (srcPadPeer); 
    gst_object_unref (sinkPadPeer); 

    //unblocking data stream 
    //g_print ("setting pad to unblocked\n"); 
    gst_pad_set_blocked_async (pad, FALSE,echo_pad_unblocked,NULL); 
}

void 
on_client_connected (GstElement * shmsink, gint num, gpointer user_data) 
{ 
    //g_print ("on client connected %d\n",num); 
   GstPad *serializerSinkPad=gst_element_get_static_pad(serializer,"sink");
   GstPad *padToBlock = gst_pad_get_peer(serializerSinkPad);
    
   if(!gst_pad_set_blocked_async (padToBlock, TRUE, switch_to_new_serializer, serializer))
       g_printerr("Error: requesting the pad to be blocked\n");
   gst_object_unref (serializerSinkPad);
   gst_object_unref (padToBlock);
} 

void 
on_client_disconnected (GstElement * shmsink, gint num, gpointer user_data) 
{ 
    g_print ("shm client disconnected %d\n",num);
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

    /* Initialisation */
    gst_init (&argc, &argv);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    /* Check input arguments */
    if (argc != 2) {
	g_printerr ("Usage: %s <socket-path>\n", argv[0]);
	return -1;
    }

    /* Create gstreamer elements */
    pipeline    = gst_pipeline_new ("shared-video-writer");
    source      = gst_element_factory_make ("videotestsrc",  "video-source");
    
    camsource   = gst_element_factory_make ("v4l2src",  NULL);

    timeoverlay = gst_element_factory_make ("timeoverlay", NULL);
    tee         = gst_element_factory_make ("tee", NULL);

    qserial     = gst_element_factory_make ("queue", NULL);
    serializer  = gst_element_factory_make ("gdppay",  NULL);
    shmsink     = gst_element_factory_make ("shmsink", "shmoutput");

    qlocalxv    = gst_element_factory_make ("queue", NULL);
    imgsink     = gst_element_factory_make ("xvimagesink", NULL);

    


    if (!pipeline || !source || !timeoverlay || !tee || !qserial 
	|| !qlocalxv || !imgsink || !serializer || !shmsink) {
	g_printerr ("One element could not be created. Exiting.\n");
	return -1;
    }

    /*specifying video format*/
     /* GstCaps *videocaps;  */
     /* videocaps = gst_caps_new_simple ("video/x-raw-yuv",  */
     /* 				     "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),  */
     /* 				     "framerate", GST_TYPE_FRACTION, 30, 1,  */
     /* 				     "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,  */
     /* 				     /\* "width", G_TYPE_INT, 600,  *\/ */
     /* 				     /\* "height", G_TYPE_INT, 400,  *\/ */
     /* 				      "width", G_TYPE_INT, 1920,   */
     /* 				      "height", G_TYPE_INT, 1080,   */
     /* 				     NULL);  */

    /* we add a message handler */
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* we add all elements into the pipeline */
    gst_bin_add_many (GST_BIN (pipeline),
		      //source, 
		      camsource, 
		      timeoverlay, tee, qserial, qlocalxv, imgsink, serializer, shmsink, NULL);
 
    /* we link the elements together */
    //gst_element_link_filtered (source, timeoverlay,videocaps);
    gst_element_link (camsource,timeoverlay);
    gst_element_link_many (timeoverlay, tee, qserial, serializer, shmsink,NULL);
    gst_element_link_many (tee, qlocalxv,imgsink,NULL);

    /* Set up the pipeline */
    g_object_set (G_OBJECT (shmsink), "socket-path", argv[1], NULL);
    g_object_set (G_OBJECT (shmsink), "shm-size", 94967295, NULL);
    g_object_set (G_OBJECT (shmsink), "sync", FALSE, NULL);
    g_object_set (G_OBJECT (shmsink), "wait-for-connection", FALSE, NULL);
        
    g_object_set (G_OBJECT (imgsink), "sync", FALSE, NULL);

    g_signal_connect (shmsink, "client-connected", G_CALLBACK (on_client_connected), NULL);
    g_signal_connect (shmsink, "client-disconnected", G_CALLBACK (on_client_disconnected), NULL);

    //adding a probe for reseting timestamp when reconnecting
    GstPad *qserialPad = gst_element_get_pad (qserial, "src");
    gst_pad_add_data_probe (qserialPad,G_CALLBACK (reset_time),NULL);
    gst_object_unref(qserialPad);

    /* Set the pipeline to "playing" state*/
    g_print ("Now writing: %s\n", argv[1]);
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
