#include <loudmouth/loudmouth.h>
#include <string.h>

#include "parse.h"

int parse_jingle(LmMessageNode *node, struct info_jingle *ij)
{
  int nb_reason = 0;
  LmMessageNode *child = NULL;
  
  if (!strcmp(ij->name, "jingle"))
    return PARSE_ERROR_NAME;

  ij->action    = g_strdup(lm_message_node_get_attribute(node, "action"));
  ij->initiator = g_strdup(lm_message_node_get_attribute(node, "initiator"));
  ij->responder = g_strdup(lm_message_node_get_attribute(node, "responder"));
  ij->sid       = g_strdup(lm_message_node_get_attribute(node, "sid"));
   
  // check required
  if (ij->action == NULL || ij->sid == NULL)
    return PARSE_ERROR_REQUIRED;
    
  // check restrictions
  if (!check_restriction(ij->action, {"content-accept", "content-add",
  "content-modify", "content-reject", "content-remove", "description-info",
  "security-info", "session-accept", "session-info", "session-initiate",
  "session-terminate", "transport-accept", "transport-info", "transport-reject",
  "transport-replace", NULL}))
   return PARSE_ERROR_RESTRICTION;
  
  // check childs
  for (child = node->children; child; child = child->next) {
    if (!strcmp(child->name, "reason"))
      nb_reason++;
  }
  
  if (reason > 1)
    return PARSE_ERROR_TOO_MANY_CHILDS;
    
  return PARSE_OK;
}


void free_jingle(struct info_jingle *ij)
{
  g_free(ij->action);
  g_free(ij->initiator);
  g_free(ij->responder);
  g_free(ij->sid);
}


int parse_content(LmMessageNode* node, struct info_content* ic)
{
  if (!strcmp(ic->name, "content"))
    return PARSE_ERROR_NAME;

  ic->creator     = g_strdup(lm_message_node_get_attribute(node, "creator"));
  ic->disposition = g_strdup(lm_message_node_get_attribute(node, "disposition"));
  ic->name        = g_strdup(lm_message_node_get_attribute(node, "name"));
  ic->senders     = g_strdup(lm_message_node_get_attribute(node, "senders"));

  // Put default if none
  if (ic->disposition == NULL)
    ic->disposition = g_strdup("session");

  // check required
  if (ic->creator == NULL || ic->name == NULL)
    return PARSE_ERROR_REQUIRED;

  // check restrictions
  if (!check_restriction(ic->creator, {"initiator", "responder", NULL}))
    return PARSE_ERROR_RESTRICTION;
  if (!check_restriction(ic->senders, {"both", "initiator", "none", "responder", NULL}))
    ic->senders = NULL; // because it's optional
    
  return PARSE_OK;
}


void free_content(struct info_content *ic)
{
  g_free(ic->creator);
  g_free(ic->disposition);
  g_free(ic->name);
  g_free(ic->senders);
}


int check_restriction(const gchar* name, const gchar** values)
{
  const char* value;
  int found = 0;
  value = values[0];
  while (value && !found) {
    if (!strcmp(name, value))
      found = 1;
    value++;
  }
  return found;
}
