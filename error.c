/**
 * Reply to a Jingle IQ with an error.
 */
void jingle_error_iq(LmMessage *m, const gchar *errtype,
    const gchar *cond, const gchar *jinglecond)
{
  LmMessage *r;
  LmMessageNode *err, *tmpnode;

  r = lm_message_new_iq_from_query(m, LM_MESSAGE_SUB_TYPE_ERROR);
  err = lm_message_node_add_child(r->node, "error", NULL);
  lm_message_node_set_attribute(err, "type", errtype);

  // error condition as defined by RFC 3920bis section 8.3.3
  tmpnode = lm_message_node_add_child(err, cond, NULL);
  lm_message_node_set_attribute(tmpnode, "xmlns", NS_XMPP_STANZAS);

  // jingle error condition as defined by XEP-0166 section 10
  tmpnode = lm_message_node_add_child(err, jinglecond, NULL);
  lm_message_node_set_attribute(tmpnode, "xmlns", NS_JINGLE_ERRORS);

  lm_connection_send(lconnection, r, NULL);
  lm_message_unref(r);
}

/**
 * Send a bad-request error (really usefull)
 */
void jingle_error_bad_request(LmMessage *m)
{
  LmMessage *r;
  LmMessageNode *err, *tmpnode;

  r = lm_message_new_iq_from_query(m, LM_MESSAGE_SUB_TYPE_ERROR);
  err = lm_message_node_add_child(r->node, "error", NULL);
  lm_message_node_set_attribute(err, "type", "cancel");

  // error condition as defined by RFC 3920bis section 8.3.3
  tmpnode = lm_message_node_add_child(err, "bad-request", NULL);
  lm_message_node_set_attribute(tmpnode, "xmlns", NS_XMPP_STANZAS);

  lm_connection_send(lconnection, r, NULL);
  lm_message_unref(r);
}
