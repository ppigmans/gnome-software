/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>

#include <gs-plugin.h>
#include <listaller.h>

struct GsPluginPrivate {
	ListallerManager	 *mgr;
};

/**
 * gs_plugin_get_name:
 */
const gchar *
gs_plugin_get_name (void)
{
	return "Listaller";
}

/**
 * gs_plugin_initialize:
 */
void
gs_plugin_initialize (GsPlugin *plugin)
{
	/* create private area */
	plugin->priv = GS_PLUGIN_GET_PRIVATE (GsPluginPrivate);
	/* new Listaller application manager in shared mode */
	plugin->priv->mgr = listaller_manager_new (TRUE);
}

/**
 * gs_plugin_get_priority:
 */
gdouble
gs_plugin_get_priority (GsPlugin *plugin)
{
	return 1.0f;
}

/**
 * gs_plugin_destroy:
 */
void
gs_plugin_destroy (GsPlugin *plugin)
{
	g_object_unref (plugin->priv->mgr);
}

/**
 * gs_plugin_add_search:
 */
gboolean
gs_plugin_add_search (GsPlugin *plugin,
		      const gchar *value,
		      GList *list,
		      GCancellable *cancellable,
		      GError **error)
{
	return TRUE;
}

/**
 * gs_plugin_add_updates:
 */
gboolean
gs_plugin_add_updates (GsPlugin *plugin,
		       GList **list,
		       GCancellable *cancellable,
		       GError **error)
{
	return TRUE;
}

/**
 * gs_plugin_add_installed:
 */
gboolean
gs_plugin_add_installed (GsPlugin *plugin,
			 GList **list,
			 GCancellable *cancellable,
			 GError **error)
{
	GsApp *app;
	ListallerAppState filter;
	GeeArrayList *app_list;
	gint i;
	ListallerAppItem *app_item;

	filter = LISTALLER_APP_STATE_AVAILABLE | LISTALLER_APP_STATE_INSTALLED_SHARED | LISTALLER_APP_STATE_INSTALLED_PRIVATE;

	listaller_manager_filter_applications (plugin->priv->mgr, filter, &app_list);
	for (i=0; i<gee_abstract_collection_get_size (GEE_ABSTRACT_COLLECTION (app_list)); i++) {
		app_item = LISTALLER_APP_ITEM (gee_abstract_list_get (GEE_ABSTRACT_LIST (app_list), i));

		g_debug ("Listaller app found: %s", listaller_app_item_get_idname (app_item));
		app = gs_app_new (listaller_app_item_get_idname (app_item));
		gs_app_set_name (app, listaller_app_item_get_full_name (app_item));
		gs_app_set_summary (app, listaller_app_item_get_summary (app_item));
		gs_app_set_state (app, GS_APP_STATE_AVAILABLE);
		gs_app_set_kind (app, GS_APP_KIND_NORMAL);
		gs_plugin_add_app (list, app);
	}

	return TRUE;
}

/**
 * gs_plugin_add_popular:
 */
gboolean
gs_plugin_add_popular (GsPlugin *plugin,
		       GList **list,
		       GCancellable *cancellable,
		       GError **error)
{
	GsApp *app;

	app = gs_app_new ("gnome-power-manager");
	gs_app_set_name (app, "Power Manager");
	gs_app_set_summary (app, "Power Management Program");
	gs_app_set_state (app, GS_APP_STATE_AVAILABLE);
	gs_app_set_kind (app, GS_APP_KIND_NORMAL);
	gs_plugin_add_app (list, app);

	return TRUE;
}

/**
 * gs_plugin_refine:
 */
gboolean
gs_plugin_refine (GsPlugin *plugin,
		  GList *list,
		  GCancellable *cancellable,
		  GError **error)
{
	GsApp *app;
	GList *l;

	for (l = list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		if (gs_app_get_name (app) == NULL) {
			if (g_strcmp0 (gs_app_get_id (app), "gnome-boxes") == 0) {
				gs_app_set_name (app, "Boxes");
				gs_app_set_summary (app, "A simple GNOME 3 application to access remote or virtual systems");
			}
		}
	}
	return TRUE;
}
