#include "lxmusic-plugin-config.h"
#include "xmmsv-utils.h"

/* known by xmms2 server */
static GList *plugins = NULL;

static void plugin_config_setup_parameter (const char* key, xmmsv_t *value, void* user_data);

GList* plugin_list() 
{
    return plugins;
}

void plugin_config_setup (xmmsc_connection_t *con) 
{
    xmmsc_result_t* res;
    xmmsv_t *val;
    const gchar *err, *name;
    
    /* call only once */
    g_assert(plugins == NULL );
    
    /* get list of available plugins */
    res = xmmsc_plugin_list (con, XMMS_PLUGIN_TYPE_OUTPUT);
    /* we havn't entered async xmmsc_mainloop, so it's ok todo sync ops */
    xmmsc_result_wait (res);
    val = xmmsc_result_get_value (res);

    if (!xmmsv_get_error (val, &err)) {
	xmmsv_list_iter_t *it;

	xmmsv_get_list_iter (val, &it);
	for (xmmsv_list_iter_first (it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) 
	{
	    xmmsv_t *elem;
	    Plugin *plugin = g_slice_new( Plugin );
	    xmmsv_list_iter_entry (it, &elem);
	    xmmsv_dict_entry_get_string (elem, "shortname", &name);
	    plugin->name = g_strdup(name);
	    xmmsv_dict_entry_get_string (elem, "description", &name);
	    plugin->display_name = g_strdup(name);
	    plugin->config = NULL;
	    plugins = g_list_append(plugins, plugin);
	}
    } else {
	g_error (_("Server error: %s\n"), err);
    }
    xmmsc_result_unref (res);

    /* get configuration options */
    res = xmmsc_configval_list (con );
    xmmsc_result_wait (res);
    val = xmmsc_result_get_value (res);
    xmmsv_dict_foreach( val, plugin_config_setup_parameter, NULL);
        
}

/* get configuration settings available for output plugins */
static void plugin_config_setup_parameter (const char* key, xmmsv_t *value, void* user_data)
{
    int i;
    for ( i = 0; i < g_list_length( plugins ); i++ ) 
    {
	Plugin* plugin = plugin_nth ( i );
	/* find plugin corresponding to config name (PREFIX.*) */
	if ( g_str_has_prefix (key, plugin->name ) ) 
	{
	    PluginConfig* config = g_slice_new( PluginConfig );
	    config->name = g_strdup( key );
	    /* to receive later from xmms2 server */
	    config->value = NULL; 
	    config->entry = NULL;
	    /* generate display_name */
	    config->display_name = g_strconcat( g_strrstr( config->name, "." ) + 1, ":", NULL );
	    plugin->config = g_list_append( plugin->config, config );
	}
    }
}

int plugin_config_widget(xmmsv_t* value, void* user_data) 
{
    PluginConfig* config = ( PluginConfig* ) user_data;
    const gchar *val;
    
    if( xmmsv_get_string(value, &val) )
    {
	g_free( config->value );
	config->value = g_strdup( val );
	gtk_entry_set_text( GTK_ENTRY( config->entry ), config->value );
    }
    return FALSE;
}

