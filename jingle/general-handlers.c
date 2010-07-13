/*
 * general-handlers.c
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

#include <mcabber/events.h>
#include <mcabber/logprint.h>
#include <mcabber/xmpp_helper.h>

#include <jingle/jingle.h>
#include <jingle/general-handlers.h>

GSList *ack_wait = NULL;

extern LmMessageHandler* jingle_ack_iq_handler;
gboolean evscallback_jingle(guint evcontext, const gchar *arg,
                            gpointer userdata)
{
  JingleNode *jn = userdata;

  // Demande à mcKael l'utilité de ce truc
  /*
  if (G_UNLIKELY(!jn)) {
    scr_LogPrint(LPRINT_LOGNORM, "Error in evs callback.");
    return FALSE;
  }
  */

  if (evcontext == EVS_CONTEXT_TIMEOUT) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s timed out, cancelled.",
                 jn->initiator);
    jingle_free_jinglenode(jn);
    return FALSE;
  }
  if (evcontext = EVS_CONTEXT_CANCEL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s cancelled.",
                 jn->initiator);
    jingle_free_jinglenode(jn);
    return FALSE;
  }
  if (!(evcontext == EVS_CONTEXT_ACCEPT || evcontext == EVS_CONTEXT_REJECT)) {
    jingle_free_jinglenode(jn);
    return FALSE;
  }
  
  if (evcontext == EVS_CONTEXT_ACCEPT) {
    jingle_send_session_accept(jn);
  } else {
    jingle_send_session_terminate(jn, "decline");
    jingle_free_jinglenode(jn);
  }

  return FALSE;
}

LmHandlerResult jingle_handle_ack_iq (LmMessageHandler *handler,
                                      LmConnection *connection, 
                                      LmMessage *message, gpointer user_data)
{
  GSList *child;
  const gchar *id = lm_message_get_id(message);
  ack_iq *ai;
  for (child = ack_wait; child; child = child->next) {
    ai = (ack_iq*)child->data;
    if(!g_strcmp0(ai->id, id)) {
      g_free(ai->id);
      if(ai->callback != NULL)
        ai->callback(message, ai->udata);
      ack_wait = g_slist_remove(ack_wait, child);
      return LM_HANDLER_RESULT_REMOVE_MESSAGE;
    }
  }
  return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

void add_ack_wait(ack_iq *elem)
{
  ack_wait = g_slist_append(ack_wait, elem);
}
