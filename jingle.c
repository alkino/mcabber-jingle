/*
 * pep.c                -- Common pep routines
 *
 * Copyrigth (C) 2009      Myhailo Danylenko <isbear@ukrpost.net>
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
#include <loudmouth/loudmouth.h>
#include <string.h>

#include <mcabber/xmpp.h>
#include <mcabber/hooks.h>
#include <mcabber/modules.h>
#include <mcabber/logprint.h>

#define NS_JINGLE "urn:xmpp:jingle:1"

struct info_iq {
  const gchar *from, *to, *type, *id;
};
struct info_jingle {
  const gchar *xmlns, *action, *initiator, *sid;
};

static void mcjingle_init   (void);
static void mcjingle_uninit (void);

void parse_iq (LmMessageNode *iq, struct info_iq *ii) {
  if(!strcmp(iq->name, "iq")) {
    ii->to = ii->from = ii->type = ii->id = NULL;
    return;
  }

  ii->from = lm_message_node_get_attribute (iq, "from");
  ii->to   = lm_message_node_get_attribute (iq, "to");
  ii->type = lm_message_node_get_attribute (iq, "type");
  ii->id   = lm_message_node_get_attribute (iq, "id");
}
void parse_jingle (LmMessageNode *jingle, struct info_jingle *ij) {
  if(!strcmp(ij->name, "jingle")) {
    ij->to = ij->from = ij->type = ij->id = NULL;
    return;
  }

  ii->from = lm_message_node_get_attribute (iq, "from");
  ii->to   = lm_message_node_get_attribute (iq, "to");
  ii->type = lm_message_node_get_attribute (iq, "type");
  ii->id   = lm_message_node_get_attribute (iq, "id");
}


LmMessageHandler* lm = NULL;

module_info_t info_jingle = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = MCABBER_VERSION,
  .description     = "A module for jingle",
  .requires        = NULL,
  .init            = mcjingle_init,
  .uninit          = mcjingle_uninit,
  .next            = NULL,
};

LmHandlerResult gestionnaire_lm(LmMessageHandler *handler,
                                LmConnection *connection, LmMessage *message,
                                gpointer user_data)
{
  LmMessageNode * test = lm_message_get_node(message)->children;
  struct info_iq ii;
  parse_iq(lm_message_get_node(message), &ii);
  if(!strcmp(test->name, "jingle")) {
    if(!strcmp(lm_message_node_get_attribute(test, "xmlns"), NS_JINGLE)) {
    	gchar* action = g_strdup(lm_message_node_get_attribute(test, "action"));
    	gchar* initiator = g_strdup(lm_message_node_get_attribute(test, "initiator"));
    	gchar* responder = g_strdup(lm_message_node_get_attribute(test, "responder"));
    	gchar* sid = g_strdup(lm_message_node_get_attribute(test, "sid"));
    	if(!strcmp(action, "content-accept")) {
     	} else if (!strcmp(action, "content-add")) {
   	} else if (!strcmp(action, "content-modify")) {
   	} else if (!strcmp(action, "content-reject")) {
   	} else if (!strcmp(action, "content-remove")) {
   	} else if (!strcmp(action, "description-info")) {
   	} else if (!strcmp(action, "security-info")) {
   	} else if (!strcmp(action, "session-accept")) {
   	} else if (!strcmp(action, "session-info")) {
   	} else if (!strcmp(action, "session-initiate")) {
   	  LmMessageNode *child = NULL;
   	  gchar* disposition = NULL;
   	  for(child = test->children; child; child = child->next) {
   	    if(!strcmp((disposition = lm_message_node_get_attribute(child, "disposition")), "session"))
   	      break;
   	    if(!disposition) {
   	      disposition = g_strdup("session");
   	      break;
   	    }
   	  }
   	  if(strcmp(disposition, "session")) {
   	    //ERREUR
   	  }
   	  
   	} else if (!strcmp(action, "session-terminate")) {
     	} else if (!strcmp(action, "transport-accept")) {
     	} else if (!strcmp(action, "transport-info")) {
     	} else if (!strcmp(action, "transport-reject")) {
     	} else if (!strcmp(action, "transport-replace")) {
      } else {
  		  { // action inconnue => réponse XEP 0166 : 7.2

  		  
  		  } 		
  		}
    } else {
      scr_log_print(LPRINT_NORMAL, "jingle : Namespace inconnu (%s)", lm_message_node_get_attribute(test, "xmlns"));
    }
  }
  
  return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}
                                                         
/* Initialization */
static void mcjingle_init(void)
{
  lm = lm_message_handler_new(gestionnaire_lm, NULL, NULL);
  lm_connection_register_message_handler (lconnection, lm, LM_MESSAGE_TYPE_IQ, LM_HANDLER_PRIORITY_FIRST);
}

/* Uninitialization */
static void mcjingle_uninit(void)
{
  lm_connection_unregister_message_handler (lconnection, lm, LM_MESSAGE_TYPE_IQ);
  lm_message_handler_invalidate (lm);
  lm_message_handler_unref (lm);
}
