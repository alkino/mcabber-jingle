/*
 * filetransfert.c
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

#include "config.h"

#include <glib.h>

#include <mcabber/modules.h>
#include <mcabber/xmpp_helper.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/register.h>

#include "filetransfert.h"


void jingle_ft_check();
static void jingle_ft_init(void);
static void jingle_ft_uninit(void);


const gchar *deps[] = { "jingle", NULL };

JingleAppFuncs funcs = {jingle_ft_check, NULL};

module_info_t info_jingle_filetransfert = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle File Transfert (XEP-0234)\n",
  .requires        = deps,
  .init            = jingle_ft_init,
  .uninit          = jingle_ft_uninit,
  .next            = NULL,
};


void jingle_ft_check(JingleContentNode *cn, GError **err, gpointer *data)
{
  return;
}

static void jingle_ft_init(void)
{
  jingle_register_app(NS_JINGLE_APP_FT, &funcs, NULL);
  xmpp_add_feature(NS_JINGLE_APP_FT);
}

static void jingle_ft_uninit(void)
{
  xmpp_del_feature(NS_JINGLE_APP_FT);
  jingle_unregister_app(NS_JINGLE_APP_FT);
}
