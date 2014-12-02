#include <gst/gst.h>

int
main (int argc,
      char *argv[])
{
    gst_init (&argc, &argv);
    
    GstElementFactory *factory = gst_element_factory_find("ffmpegcolorspace"); 
    const GList *list = gst_element_factory_get_static_pad_templates(factory);  
    while (NULL != list) {  
      GstStaticPadTemplate *templ = (GstStaticPadTemplate *)list->data;  
      // name  
      g_print("+++ template name %s\n", templ->name_template);  
      // direction  
      g_print ("direction: ");  
      switch (templ->direction) {  
        case GST_PAD_UNKNOWN:  
          g_print ("unknown\n");  
          break;  
        case GST_PAD_SRC:  
          g_print ("src\n");  
          break;  
        case GST_PAD_SINK:  
          g_print ("sink\n");  
          break;  
        default:  
          g_print ("this is a bug\n");  
          break;  
      }  

      // presence  
      g_print ("presence: ");  
      switch (templ->presence) {  
        case GST_PAD_ALWAYS:  
          g_print ("always\n");  
          break;  
        case GST_PAD_SOMETIMES:  
          g_print ("sometimes\n");  
          break;  
        case GST_PAD_REQUEST:  
          g_print ("request\n");  
          break;  
        default:  
          g_print ("this is a bug\n");  
          break;  
      }  

      // caps  
      GstCaps *caps = gst_static_caps_get(&templ->static_caps);  
      // copying for removing fields in struture  
      GstCaps *copy = gst_caps_copy(caps);  
      gst_caps_unref(caps);  

      guint size = gst_caps_get_size(copy);  
      guint i = 0;  
      g_print("size %u\n", size);  
      for (; i < size; i++) {  
        GstStructure *structure = gst_caps_get_structure(copy, i);  
        gst_structure_remove_fields(structure,  
                                    "format", "width", "height", "framerate",  
                                    NULL);  
        GstCaps *copy_nth = gst_caps_copy_nth(copy, i);   
        gchar *caps_str = gst_caps_to_string(copy_nth);   
        g_print("    caps num %u is %s\n", i, caps_str);   
        g_free(caps_str);   
        gst_caps_unref(copy_nth);   
      }  
      gst_caps_unref(copy);  
      list = g_list_next(list);  
    }  
    gst_object_unref(factory); 

    gst_deinit();  // for memory testing  
    return 0;
}
