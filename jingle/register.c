/*
 * register.c
 *
 * Copyrigth (C) 2010 Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <glib.h>

#include <mcabber/logprint.h>

#include <jingle/register.h>
#include <jingle/jingle.h>


typedef struct {
  gchar *xmlns;
  JingleAppFuncs *funcs;
  JingleTransportType transtype;
} AppHandlerEntry;

typedef struct {
  gchar *xmlns;
  JingleTransportFuncs *funcs;
  JingleTransportType transtype;
  JingleTransportPriority priority;
} TransportHandlerEntry;


static AppHandlerEntry *jingle_find_app(const gchar *xmlns);
static TransportHandlerEntry *jingle_find_transport(const gchar *xmlns);


GSList *jingle_app_handlers = NULL;
GSList *jingle_transport_handlers = NULL;


/**
 * Register a new supported application.
 * type is the type of transport the data shall be sent over.
 */
void jingle_register_app(const gchar *xmlns, JingleAppFuncs *funcs,
                         JingleTransportType type)
{
  if (!g_str_has_prefix(xmlns, NS_JINGLE_APP_PREFIX)) return;

  AppHandlerEntry *h = g_new(AppHandlerEntry, 1);

  h->xmlns       = g_strdup(xmlns);
  h->funcs       = funcs;
  h->transtype   = type;

  jingle_app_handlers = g_slist_append(jingle_app_handlers, h);
}

/**
 * Register a new supported transport.
 * type is the type of transport.
 */
void jingle_register_transport(const gchar *xmlns,
                               JingleTransportFuncs *funcs,
                               JingleTransportType type,
                               JingleTransportPriority prio)
{
  if (!g_str_has_prefix(xmlns, NS_JINGLE_TRANSPORT_PREFIX)) return;

  TransportHandlerEntry *h = g_new(TransportHandlerEntry, 1);

  h->xmlns     = g_strdup(xmlns);
  h->funcs     = funcs;
  h->transtype = type;
  h->priority  = prio;

  jingle_transport_handlers = g_slist_append(jingle_transport_handlers, h);
}

JingleAppFuncs *jingle_get_appfuncs(const gchar *xmlns)
{
  AppHandlerEntry *entry;
  return (entry = jingle_find_app(xmlns)) != NULL ? entry->funcs : NULL;
}

JingleTransportFuncs *jingle_get_transportfuncs(const gchar *xmlns)
{
  TransportHandlerEntry *entry;
  return (entry = jingle_find_transport(xmlns)) != NULL ? entry->funcs : NULL;
}

gint cmp_forbid(gconstpointer a, gconstpointer b)
{
  return g_strcmp0((const gchar *)a, (const gchar *)b);
}

/**
 * Determine which transport is better suited for a given app.
 */
const gchar *jingle_transport_for_app(const gchar *appxmlns,
                                               GSList **forbid)
{
  AppHandlerEntry *app = jingle_find_app(appxmlns);
  GSList *entry;
  TransportHandlerEntry *thistransport, *besttransport = NULL;
  JingleTransportPriority bestprio = JINGLE_TRANSPORT_NONE;
  JingleTransportType requestedtype;

  if (app == NULL)
    return NULL;

  requestedtype = app->transtype;
  for (entry = jingle_transport_handlers; entry; entry = entry->next) {
    thistransport = (TransportHandlerEntry *) entry->data;
    
    // Look if it's forbidden
    if (forbid != NULL &&
        g_slist_find_custom(*forbid, thistransport->xmlns, cmp_forbid))
      continue;
    
    if (thistransport->transtype == requestedtype &&
        thistransport->priority > bestprio) {
      bestprio = thistransport->priority;
      besttransport = thistransport;
    }
  }
  
  if (forbid != NULL)
    *forbid = g_slist_append(*forbid, besttransport->xmlns);
  
  return besttransport != NULL ? besttransport->xmlns : NULL;
}

/**
 * This function should work with AppHandlerEntry and
 * TransportHandlerEntry as long as both start with xmlns.
 */
static gint jingle_entry_cmp(gconstpointer a, gconstpointer b)
{
  return g_strcmp0(((AppHandlerEntry *) a)->xmlns, (const gchar*) b);
}

static AppHandlerEntry *jingle_find_app(const gchar *xmlns)
{
  GSList *entry = g_slist_find_custom(jingle_app_handlers, xmlns,
                                      jingle_entry_cmp);
  return entry != NULL ? (AppHandlerEntry *) entry->data : NULL;
}

static TransportHandlerEntry *jingle_find_transport(const gchar *xmlns)
{
  GSList *entry = g_slist_find_custom(jingle_transport_handlers, xmlns,
                                      jingle_entry_cmp);
  return entry != NULL ? (TransportHandlerEntry *) entry->data : NULL;
}

static void jingle_free_app(AppHandlerEntry *entry)
{
  g_free(entry->xmlns);
  g_free(entry);
}

static void jingle_free_transport(TransportHandlerEntry *entry)
{
  g_free(entry->xmlns);
  g_free(entry);
}

void jingle_unregister_app(const gchar *xmlns)
{
  AppHandlerEntry *entry = jingle_find_app(xmlns);
  if (entry) {
    jingle_free_app(entry);
    jingle_app_handlers = g_slist_remove(jingle_app_handlers, entry);
  }
}

void jingle_unregister_transport(const gchar *xmlns)
{
  TransportHandlerEntry *entry = jingle_find_transport(xmlns);
  if (entry) {
    jingle_free_transport(entry);
    jingle_transport_handlers = g_slist_remove(jingle_app_handlers, entry);
  }
}
