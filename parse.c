#include <loudmouth/loudmouth.h>
#include <string.h>

#include "parse.h"
#include "jingle.h"

const gchar *jingle_actions[] = {
  "content-accept",
  "content-add",
  "content-modify",
  "content-reject",
  "content-remove",
  "description-info",
  "security-info",
  "session-accept",
  "session-info",
  "session-initiate",
  "session-terminate",
  "transport-accept",
  "transport-info",
  "transport-reject",
  "transport-replace",
  NULL
};

const gchar *jingle_content_creator[] = {
  "initiator",
  "responder",
  NULL
};

const gchar *jingle_content_senders[] = {
  "both",
  "initiator",
  "none",
  "responder",
  NULL
};

/**
 * Populate a jingle_data struct from a <jingle> element.
 * Check if the element is in compliance with the XEP.
 */
int check_jingle(LmMessageNode *node, struct jingle_data *ij)
{
  int nb_reason = 0;
  LmMessageNode *child = NULL;

  ij->action    = lm_message_node_get_attribute(node, "action");
  ij->initiator = lm_message_node_get_attribute(node, "initiator");
  ij->responder = lm_message_node_get_attribute(node, "responder");
  ij->sid       = lm_message_node_get_attribute(node, "sid");

  if (ij->action == NULL || ij->sid == NULL)
    return PARSE_ERROR_REQUIRED; // those elements are required

  if (!str_in_array(ij->action, jingle_actions))
   return PARSE_ERROR_RESTRICTION;

  // check childs
  for (child = node->children; child; child = child->next) {
    if (!strcmp(child->name, "reason"))
      nb_reason++;
  }

  if (nb_reason > 1)
    return PARSE_ERROR_TOO_MANY_CHILDS;

  return PARSE_OK;
}

int parse_content(LmMessageNode* node, struct content_data* ic)
{
  if (!strcmp(ic->name, "content"))
    return PARSE_ERROR_NAME;

  ic->creator     = lm_message_node_get_attribute(node, "creator");
  ic->disposition = lm_message_node_get_attribute(node, "disposition");
  ic->name        = lm_message_node_get_attribute(node, "name");
  ic->senders     = lm_message_node_get_attribute(node, "senders");

  if (ic->disposition == NULL)
    ic->disposition = "session";

  if (ic->creator == NULL || ic->name == NULL)
    return PARSE_ERROR_REQUIRED;

  if (!str_in_array(ic->creator, jingle_content_creator))
    return PARSE_ERROR_RESTRICTION;
  if (!str_in_array(ic->senders, jingle_content_senders))
    ic->senders = NULL;

  return PARSE_OK;
}

/**
 * Check if needle exists in haystack.
 * The last element of haystack must be NULL.
 */
gint str_in_array(const gchar* needle, const gchar** haystack)
{
  const gchar* value;
  gint found = 0;
  for (value = haystack[0]; value && !found; value++)
    if (!g_strcmp0(needle, value))
      found = 1;

  return found;
}
