/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "apphook.h"
#include "messages.h"
#include "children.h"
#include "dnscache.h"
#include "alarms.h"
#include "stats.h"
#include "tags.h"
#include "logmsg.h"

#include <time.h>

typedef struct _ApplicationHookEntry
{
  gint type;
  ApplicationHookFunc func;
  gpointer user_data;
} ApplicationHookEntry;

static GList *application_hooks = NULL;
static gint current_state = AH_STARTUP;

void 
register_application_hook(gint type, ApplicationHookFunc func, gpointer user_data)
{
  if (current_state < type)
    {
      ApplicationHookEntry *entry = g_new0(ApplicationHookEntry, 1);
      
      entry->type = type;
      entry->func = func;
      entry->user_data = user_data;
      
      application_hooks = g_list_prepend(application_hooks, entry);
    }
  else
    {
      /* the requested hook has already passed, call the requested function immediately */
      msg_debug("Application hook registered after the given point passed", 
                evt_tag_int("current", current_state), 
                evt_tag_int("hook", type), NULL);
      func(type, user_data);
    }
}

static void
run_application_hook(gint type)
{
  GList *l, *l_next;
  
  g_assert(current_state <= type);
  
  msg_debug("Running application hooks", evt_tag_int("hook", type), NULL);
  current_state = type;
  for (l = application_hooks; l; l = l_next)
    {
      ApplicationHookEntry *e = l->data;
      
      if (e->type == type)
        {
          l_next = l->next;
          application_hooks = g_list_remove_link(application_hooks, l);
          e->func(type, e->user_data);
          g_free(e);
          g_list_free_1(l);
        }
      else
        {
          l_next = l->next;
        }
    }

}

void 
app_startup(void)
{
#if ENABLE_THREADS
  g_thread_init(NULL);
#endif
  msg_init(FALSE);
  child_manager_init();
  dns_cache_init();
  alarm_init();
  stats_init();
  tzset();
  log_msg_global_init();
  log_tags_init();
}

void
app_post_daemonized(void)
{
  run_application_hook(AH_POST_DAEMONIZED);
}

void
app_pre_config_loaded(void)
{
  current_state = AH_PRE_CONFIG_LOADED;
}

void
app_post_config_loaded(void)
{
  run_application_hook(AH_POST_CONFIG_LOADED);
}

void 
app_shutdown(void)
{
  run_application_hook(AH_SHUTDOWN);
  stats_destroy();
  dns_cache_destroy();
  child_manager_deinit();
  msg_deinit();
  log_tags_deinit();
  g_list_free(application_hooks);
}

