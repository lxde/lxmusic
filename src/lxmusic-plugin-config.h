#ifndef LXMUSIC_PLUGIN_CONFIG_H
#define LXMUSIC_PLUGIN_CONFIG_H

#include <gtk/gtk.h>
#include <xmmsclient/xmmsclient.h>
#include <glib/gi18n.h>

/* blacklists "not useful" for end users */
#define PLUGIN_BLACKLIST "null ices"

typedef struct _LXMusicPlugin 
{
    gchar *name;
    GList *config;
} Plugin;
    

typedef struct _LXMusicPluginConfig {
    gchar *name;		/* configuration parameter name */
    gchar *value;		/* current configuration value */
    GtkWidget *entry;
    Plugin *plugin;	/* corresponding plugin */
} PluginConfig;

GList* plugin_list();
static inline Plugin* plugin_nth(int i) {
    return ( Plugin* ) i>=0?g_list_nth_data( plugin_list(), i ):NULL;
}
static inline PluginConfig* plugin_config_nth( Plugin *plugin, int i ) 
{
    return (PluginConfig*) i>=0?g_list_nth_data( plugin->config, i ):NULL;
}
const gchar* plugin_config_gettext(const gchar *lookup) ;
void plugin_config_setup (xmmsc_connection_t *con);
int plugin_config_widget(xmmsv_t* value, void* user_data);
void plugin_config_widget_setup_hbox( GtkWidget* output_config_hbox );

#endif
