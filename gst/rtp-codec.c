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
		    g_print ("string: %s\n",   
			     gst_structure_to_string (caps_struct));   
		    

		    {//payload 
		      const GValue *val = gst_structure_get_value (caps_struct, "payload");  
		      if (NULL != val) 
			{ 
			  //g_print ("payload struct type %s\n", G_VALUE_TYPE_NAME (val));  
			  if(GST_VALUE_HOLDS_INT_RANGE(val)) 
			    { 
			      g_print ("payload min %d\n", gst_value_get_int_range_min (val));  
			    } 
			  if (GST_VALUE_HOLDS_LIST(val)) 
			    { 
			      for (guint i = 0; i < gst_value_list_get_size (val); i++) 
				{ 
				  const GValue *item_val = gst_value_list_get_value (val, i); 
				  g_print ("payload list %d\n", g_value_get_int (item_val)); 
				} 
			    } 
			  if (G_VALUE_HOLDS_INT (val)) 
			    { 
			      g_print ("payload int %d\n", g_value_get_int (val)); 
			    } 
			} 
		    } 
		    { //encodeing-name
		      const GValue *val = gst_structure_get_value (caps_struct, "encoding-name");  
		      if (NULL != val) 
			{
			  //g_print ("encoding-name struct type %s\n", G_VALUE_TYPE_NAME (val));  
			  if (GST_VALUE_HOLDS_LIST(val)) 
			    { 
			      for (guint i = 0; i < gst_value_list_get_size (val); i++) 
				{ 
				  const GValue *item_val = gst_value_list_get_value (val, i); 
				  g_print ("encoding-name list %s\n", g_value_get_string (item_val)); 
				} 
			    } 
			  if (G_VALUE_HOLDS_STRING (val)) 
			    { 
			      g_print ("encoding-name string %s\n", g_value_get_string (val)); 
			    } 
				      
			}
		    } 
		    /* g_print ("\nencoding-name %s\n",   */
		    /* 	 gst_structure_get_string (caps_struct,  */
		    /* 				   "encoding-name"));  */
			
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
