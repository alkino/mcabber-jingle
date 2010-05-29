#include <string.h>
#include <jingle.h>

GHashTable *hk_jingle_apps_handler_hash = NULL;
GHashTable *hk_jingle_transports_handler_hash = NULL;

gchar* jingle_register_apps(const gchar* namespace)
{
  gchar* hookname = NULL;

  // On l'enregistre dans la table
  // Si la table n'est pas encore créé c'est le moment
  if (!hk_jingle_apps_handler_hash) {
    hk_jingle_apps_handler_hash = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, &g_free);
    if (!hk_jingle_apps_hander_hash) {
      scr_log_print(LPRINT_LOGNORM, "Couldn't create hook hash table for jingle apps!");
      return NULL;
    }
  }

  hookname = g_strdup_printf("%s%s", "hook_jingle_apps_", namespace);

  // On insert la donnée
  g_hash_table_insert(hk_jingle_apps_handler_hash, g_strdup(namespace), hook_name);

  return hookname;
}

gchar* jingle_register_transports(const gchar* namespace) {
  gchar* hookname = NULL;

  // On l'enregistre dans la table
  // Si la table n'est pas encore créé c'est le moment
  if (!hk_jingle_transports_handler_hash) {
    hk_jingle_transports_handler_hash = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, &g_free);
    if (!hk_jingle_transports_handler_hash) {
      scr_log_print(LPRINT_LOGNORM, "Couldn't create hook hash table for jingle transports!");
      return NULL;
    }
  }

  hookname = g_strdup_printf("%s%s", "hook_jingle_transports_", namespace);

  // On insert la donnée
  g_hash_table_insert(hk_jingle_transporst_handler_hash, g_strdup(namespace), hook_name);

  return hookname;
}
