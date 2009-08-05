#include "lxmusic-plugin-config.h"

/* known by xmms2 server */
static GList *plugins = NULL;

static GHashTable* translation = NULL;
static void plugin_config_init_translation()
{
    /* Known plugin/config names to get list of possible strings
     * names useful for desktop users and translators */
    g_return_if_fail( translation == NULL );
    translation = g_hash_table_new( g_str_hash, g_str_equal );

    /* alsa use: 
     * # xmms2 config_list|egrep "^alsa" 
     * to get list of possible config variables */
    g_hash_table_insert( translation, ( gpointer ) "alsa", _("Advanced Linux Sound Architecture" ));
    g_hash_table_insert( translation, ( gpointer ) "alsa.device", _("Device:" ));
    g_hash_table_insert( translation, ( gpointer ) "alsa.mixer", _("Mixer:" ));

    /* oss */
    g_hash_table_insert( translation, ( gpointer ) "oss", _("Open Sound System" ));    
    g_hash_table_insert( translation, ( gpointer ) "oss.device", _("Device:" ));
    g_hash_table_insert( translation, ( gpointer ) "oss.mixer", _("Mixer:" ));

    /* ao  */
    g_hash_table_insert( translation, ( gpointer ) "ao", _( "Cross-platform audio output library" ) );
    g_hash_table_insert( translation, ( gpointer ) "ao.device", _("Device:" ));

    /* pulse */
    g_hash_table_insert( translation, ( gpointer ) "pulse", _("PulseAudio" ) );
    g_hash_table_insert( translation, ( gpointer ) "pulse.name", _("Name:" ));
    g_hash_table_insert( translation, ( gpointer ) "pulse.server", _("Server:" ));    
    g_hash_table_insert( translation, ( gpointer ) "pulse.sink", _("Sink:" ));    

    /* airtunes */
    g_hash_table_insert( translation, ( gpointer ) "airplay",   _( "AirTunes" ));
    g_hash_table_insert( translation, ( gpointer ) "airplay.airport_address", _("Server:" ));

    /* jack */
    g_hash_table_insert( translation, ( gpointer ) "jack",   _( "JACK Audio Connection Kit" ) );

    g_hash_table_insert( translation, ( gpointer ) "diskwrite", _( "Disk Writer" ) );
    g_hash_table_insert( translation, ( gpointer ) "diskwrite.destination_directory", _( "Destination Directory:" ) );    
	
}

const gchar* plugin_config_gettext(const gchar *lookup) 
{
    g_return_val_if_fail( translation != NULL, NULL );
    const gchar *lookup_key =  g_hash_table_lookup( translation, lookup  );
    const gchar *message = _(lookup_key);
    return message;
}


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

    /* get translation of plugins/configs */
    plugin_config_init_translation();

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
	    Plugin *plugin;
	    xmmsv_list_iter_entry (it, &elem);
	    xmmsv_dict_entry_get_string (elem, "shortname", &name);

	    /* not blacklisted */
	    if ( g_strrstr( PLUGIN_BLACKLIST, name ) != NULL ) 
		continue;

	    plugin = g_slice_new( Plugin );
	    plugin->name = g_strdup(name);
	    plugin->config = NULL;
	    plugins = g_list_append(plugins, plugin);
	}
    } else {
	g_error ( "Server error: %s", err);
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

