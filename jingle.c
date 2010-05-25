/*
 *  jingle.c -- Base jingle functions
 *
 * Copyrigth (C) 2010    Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>
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

#include "jingle.h"
#include "parse.h"


static void mcjingle_init  (void);
static void mcjingle_uninit(void);


struct info_iq {
	const gchar *from, *to, *type, *id;
};

LmMessageHandler* jingle_iq_handler = NULL;


module_info_t info_jingle = {
	.branch          = MCABBER_BRANCH,
	.api             = MCABBER_API_VERSION,
	.version         = MCABBER_VERSION,
	.description     = "Main Jingle module\n"
		" required for file transport, voip...\n",
	.requires        = NULL,
	.init            = mcjingle_init,
	.uninit          = mcjingle_uninit,
	.next            = NULL,
};


void parse_iq(LmMessageNode *iq, struct info_iq *ii)
{
	if (!strcmp(iq->name, "iq")) {
		ii->to = ii->from = ii->type = ii->id = NULL;
		return;
	}

	ii->from = lm_message_node_get_attribute(iq, "from");
	ii->to   = lm_message_node_get_attribute(iq, "to");
	ii->type = lm_message_node_get_attribute(iq, "type");
	ii->id   = lm_message_node_get_attribute(iq, "id");
}

LmHandlerResult jingle_iq_event_handler(LmMessageHandler *handler,
		LmConnection *connection,
		LmMessage *message,
		gpointer user_data)
{
	LmMessageNode *test = lm_message_get_node(message)->children;
	struct info_iq ii;
	struct info_jingle ij;

	parse_iq(lm_message_get_node(message), &ii);

	if (parse_jingle(test, &ij) == PARSE_ERROR_NAME)
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	if (strcmp(lm_message_node_get_attribute(test, "xmlns"), NS_JINGLE)) {
		scr_log_print(LPRINT_NORMAL, "jingle : Namespace inconnu (%s)", lm_message_node_get_attribute(test, "xmlns"));
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	if (!strcmp(ij.action, "content-accept")) {
	} else if (!strcmp(ij.action, "content-add")) {
	} else if (!strcmp(ij.action, "content-modify")) {
	} else if (!strcmp(ij.action, "content-reject")) {
	} else if (!strcmp(ij.action, "content-remove")) {
	} else if (!strcmp(ij.action, "description-info")) {
	} else if (!strcmp(ij.action, "security-info")) {
	} else if (!strcmp(ij.action, "session-accept")) {
	} else if (!strcmp(ij.action, "session-info")) {
	} else if (!strcmp(ij.action, "session-initiate")) {
	} else if (!strcmp(ij.action, "session-terminate")) {
	} else if (!strcmp(ij.action, "transport-accept")) {
	} else if (!strcmp(ij.action, "transport-info")) {
	} else if (!strcmp(ij.action, "transport-reject")) {
	} else if (!strcmp(ij.action, "transport-replace")) {
	}
}

static void mcjingle_init(void)
{
	jingle_iq_handler = lm_message_handler_new(jingle_iq_event_handler, NULL, NULL);
	lm_connection_register_message_handler(lconnection, jingle_iq_handler, LM_MESSAGE_TYPE_IQ, LM_HANDLER_PRIORITY_FIRST);
}

static void mcjingle_uninit(void)
{
	lm_connection_unregister_message_handler(lconnection, jingle_iq_handler, LM_MESSAGE_TYPE_IQ);
	lm_message_handler_invalidate(jingle_iq_handler);
	lm_message_handler_unref(jingle_iq_handler);
}
