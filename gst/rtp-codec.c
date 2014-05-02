#include <gst/gst.h>

int
main (int argc,
      char *argv[])
{
    
    /* Initialisation */
    gst_init (&argc, &argv);

    GList *element_list = gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_DEPAYLOADER, 
								 GST_RANK_NONE);
    GList *iter = element_list;
    while (iter != NULL)
       {
	 g_print ("+++++\n");
	 g_print ("%s -- ", gst_element_factory_get_longname ((GstElementFactory *)iter->data));
	 g_print ("%s\n", gst_plugin_feature_get_name ((GstPluginFeature *)iter->data));
	 
	 const GList *static_pads = 
	   gst_element_factory_get_static_pad_templates ((GstElementFactory *)iter->data);
	 
	 while (NULL != static_pads)
	   {
	     GstStaticPadTemplate *pad = (GstStaticPadTemplate *)static_pads->data; 
	     //the following is EMPTY gchar *caps_str = gst_caps_to_string (&pad->static_caps.caps); 
	     //g_free (caps_str); 
	     /* g_print ("string: %s\n",  */
	     /* 	      pad->static_caps.string);  */
	     GstCaps *caps = gst_caps_from_string (pad->static_caps.string);
	     guint caps_size = gst_caps_get_size (caps);
	     if (! gst_caps_is_any (caps))
	       for (guint i = caps_size; i > 0; i--) 
		 {
		   GstStructure *caps_struct = gst_caps_get_structure (caps, i-1);
		   if (gst_structure_has_name (caps_struct,"application/x-rtp")) 
		     {
		       gint payload = 0;
		       gst_structure_get_int (caps_struct, "payload", &payload);
		       g_print ("string: %s\n",  
				gst_structure_to_string (caps_struct));  
		       g_print ("encoding-name %s, payload %d\n", 
				gst_structure_get_string (caps_struct,
							  "encoding-name"),
				payload);
		     }
		 }
	     static_pads = g_list_next (static_pads); 
	     gst_caps_unref (caps);
	   }
	 
	 iter = g_list_next (iter);
       }
    gst_plugin_feature_list_free (element_list);
    
    return 0;
}
