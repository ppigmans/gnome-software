/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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

/* Notes:
 *
 * All GsApp's created have management-plugin set to XgdApp
 * Some GsApp's created have have XgdApp::type of app or runtime
 * The GsApp:source is the remote name, e.g. test-repo
 */

#include <config.h>

#include <gs-plugin.h>

struct GsPluginPrivate {
	guint			 dummy;
};

/**
 * gs_plugin_get_name:
 */
const gchar *
gs_plugin_get_name (void)
{
	return "xdg-app";
}

/**
 * gs_plugin_initialize:
 */
void
gs_plugin_initialize (GsPlugin *plugin)
{
	/* create private area */
	plugin->priv = GS_PLUGIN_GET_PRIVATE (GsPluginPrivate);
	plugin->priv->dummy = 999;
}

/**
 * gs_plugin_destroy:
 */
void
gs_plugin_destroy (GsPlugin *plugin)
{
	plugin->priv->dummy = 0;
}

/**
 * gs_plugin_xdg_app_exec:
 */
static gchar **
gs_plugin_xdg_app_exec (const gchar **args, GError **error)
{
	GPtrArray *results = NULL;
	gboolean ret;
	gint exit_status = 0;
	guint i;
	guint len;
	_cleanup_free_ gchar *standard_error = NULL;
	_cleanup_free_ gchar *standard_output = NULL;
	_cleanup_strv_free_ gchar **argv = NULL;
	_cleanup_strv_free_ gchar **lines = NULL;

	/* create argv */
	len = g_strv_length ((gchar **) args);
	argv = g_new0 (gchar *, len + 2);
	argv[0] = g_strdup ("xdg-app");
	for (i = 0; i < len; i++)
		argv[i + 1] = g_strdup (args[i]);

	/* run tool */
	ret = g_spawn_sync (NULL, (gchar **) argv, (gchar **) NULL,
			    G_SPAWN_SEARCH_PATH |
			    G_SPAWN_CLOEXEC_PIPES,
			    NULL, NULL,
			    &standard_output, &standard_error,
			    &exit_status, error);
	if (!ret)
		return NULL;
	if (exit_status != 0) {
		_cleanup_free_ gchar *tmp = g_strjoinv (" ", (gchar **) argv);
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "Failed to launch '%s': %s",
			     tmp, standard_error);
		return NULL;
	}

	/* only return valid lines */
	lines = g_strsplit (standard_output, "\n", -1);
	results = g_ptr_array_new ();
	for (i = 0; lines[i] != NULL; i++) {
		g_debug ("line %i: '%s'", i, lines[i]);
		if (lines[i][0] == '\0')
			continue;
		if (g_strstr_len (lines[i], -1, "MESSAGE") != NULL)
			continue;
		if (g_strstr_len (lines[i], -1, "DEBUG") != NULL)
			continue;
		g_ptr_array_add (results, g_strdup (lines[i]));
	}
	g_ptr_array_add (results, NULL);
	return (gchar **) g_ptr_array_free (results, FALSE);
}

/**
 * gs_plugin_add_search:
 */
gboolean
gs_plugin_add_search (GsPlugin *plugin,
		      gchar **values,
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
	const gchar *args_apps[] = { "--user", "list-apps", NULL };
	const gchar *args_runt[] = { "--user", "list-runtimes", NULL };
	guint i;
	_cleanup_strv_free_ gchar **results_apps = NULL;
	_cleanup_strv_free_ gchar **results_runt = NULL;

	/* get apps */
	results_apps = gs_plugin_xdg_app_exec (args_apps, error);
	if (results_apps == NULL)
		return FALSE;
	for (i = 0; results_apps[i] != NULL; i++) {
		_cleanup_object_unref_ GsApp *app = NULL;
		app = gs_app_new (results_apps[i]);
		gs_app_set_management_plugin (app, "XgdApp");
		gs_app_set_metadata (app, "XgdApp::type", "app");
		gs_app_set_name (app, GS_APP_QUALITY_NORMAL, "GNOME Builder");
		gs_app_set_summary (app, GS_APP_QUALITY_NORMAL, "GNOME BUILDER");
		gs_app_set_state (app, AS_APP_STATE_AVAILABLE);
		gs_app_set_kind (app, GS_APP_KIND_NORMAL);
		gs_app_set_id_kind (app, AS_ID_KIND_DESKTOP);
		//FIXME need to refine using AppData
		gs_app_set_pixbuf (app, gdk_pixbuf_new_from_file ("/usr/share/icons/hicolor/48x48/apps/gnome-boxes.png", NULL));
		gs_plugin_add_app (list, app);
	}

	/* get runtimes */
	results_runt = gs_plugin_xdg_app_exec (args_runt, error);
	if (results_runt == NULL)
		return FALSE;
	for (i = 0; results_runt[i] != NULL; i++) {
		_cleanup_object_unref_ GsApp *app = NULL;
		app = gs_app_new (results_runt[i]);
		gs_app_set_management_plugin (app, "XgdApp");
		gs_app_set_metadata (app, "XgdApp::type", "runtime");
		gs_app_set_name (app, GS_APP_QUALITY_NORMAL, "GNOME Platform");
		gs_app_set_summary (app, GS_APP_QUALITY_NORMAL, "GNOME PLATFORM");
		gs_app_set_state (app, AS_APP_STATE_AVAILABLE);
		gs_app_set_kind (app, GS_APP_KIND_NORMAL);
		gs_app_set_id_kind (app, AS_ID_KIND_DESKTOP);
		//FIXME need to refine using AppData
		gs_app_set_pixbuf (app, gdk_pixbuf_new_from_file ("/usr/share/icons/hicolor/48x48/apps/gnome-boxes.png", NULL));
		gs_plugin_add_app (list, app);
	}
	return TRUE;
}

/**
 * gs_plugin_add_sources:
 */
gboolean
gs_plugin_add_sources (GsPlugin *plugin,
		       GList **list,
		       GCancellable *cancellable,
		       GError **error)
{
	const gchar *args[] = { "--user", "list-remotes", NULL };
	guint i;
	_cleanup_strv_free_ gchar **results = NULL;

	/* get remotes */
	results = gs_plugin_xdg_app_exec (args, error);
	if (results == NULL)
		return FALSE;
	for (i = 0; results[i] != NULL; i++) {
		_cleanup_object_unref_ GsApp *app = NULL;
		app = gs_app_new (results[i]);
		gs_app_set_management_plugin (app, "XgdApp");
		gs_app_set_kind (app, GS_APP_KIND_SOURCE);
		gs_app_set_state (app, AS_APP_STATE_INSTALLED);
		//FIXME: need this from the keyfile / AppStream
		gs_app_set_name (app,
				 GS_APP_QUALITY_LOWEST,
				 results[i]);
		//FIXME: need this from the keyfile
		gs_app_set_summary (app,
				    GS_APP_QUALITY_LOWEST,
				    results[i]);
		gs_plugin_add_app (list, app);
	}
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
	//FIXME: hardcoded remote
	const gchar *args[] = { "--user", "--updates", "repo-contents",
				"test-repo", NULL };
	guint i;
	_cleanup_strv_free_ gchar **results = NULL;

	/* get updates */
	results = gs_plugin_xdg_app_exec (args, error);
	if (results == NULL)
		return FALSE;
	for (i = 0; results[i] != NULL; i++) {
		_cleanup_object_unref_ GsApp *app = NULL;
		app = gs_app_new (results[i]);
		gs_app_set_management_plugin (app, "XgdApp");
		gs_app_set_state (app, AS_APP_STATE_UPDATABLE);
		gs_app_set_kind (app, GS_APP_KIND_PACKAGE);
		gs_plugin_add_app (list, app);
		//FIXME: need this from the keyfile / AppStream
		gs_app_set_name (app,
				 GS_APP_QUALITY_LOWEST,
				 results[i]);
		//FIXME: need this from the keyfile
		gs_app_set_summary (app,
				    GS_APP_QUALITY_LOWEST,
				    results[i]);
		gs_plugin_add_app (list, app);
	}
	return TRUE;
}

/**
 * gs_plugin_refine_item:
 */
static gboolean
gs_plugin_refine_item (GsPlugin *plugin, GsApp *app, GError **error)
{
	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	//FIXME need to set to remote somehow
	gs_app_add_source (app, "test-repo");
	return TRUE;
}

/**
 * gs_plugin_refine:
 */
gboolean
gs_plugin_refine (GsPlugin *plugin,
		  GList **list,
		  GsPluginRefineFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	GList *l;
	GsApp *app;

	for (l = *list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		if (!gs_plugin_refine_item (plugin, app, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * gs_plugin_launch:
 */
gboolean
gs_plugin_launch (GsPlugin *plugin,
		  GsApp *app,
		  GCancellable *cancellable,
		  GError **error)
{
	const gchar *args[] = { "run", gs_app_get_id (app), NULL };
	_cleanup_strv_free_ gchar **results = NULL;

	/* run tool sync */
	results = gs_plugin_xdg_app_exec (args, error);
	if (results == NULL)
		return FALSE;
	return TRUE;
}

/**
 * gs_plugin_app_remove:
 */
gboolean
gs_plugin_app_remove (GsPlugin *plugin,
		      GsApp *app,
		      GCancellable *cancellable,
		      GError **error)
{
	const gchar *args[] = { "--user", "uninstall-app", gs_app_get_id (app), NULL };
	_cleanup_strv_free_ gchar **results = NULL;

	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	/* this should be much less common */
	if (g_strcmp0 (gs_app_get_metadata_item (app, "XgdApp::type"), "runtime") == 0)
		args[1] = "uninstall-runtime";

	/* run tool sync */
	results = gs_plugin_xdg_app_exec (args, error);
	if (results == NULL)
		return FALSE;
	return TRUE;
}

/**
 * gs_plugin_app_install:
 */
gboolean
gs_plugin_app_install (GsPlugin *plugin,
		       GsApp *app,
		       GCancellable *cancellable,
		       GError **error)
{
	const gchar *args[] = { "--user", "install-app",
				gs_app_get_source_default (app),
				gs_app_get_id (app), NULL };
	_cleanup_strv_free_ gchar **results = NULL;

	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	/* this should be much less common */
	if (g_strcmp0 (gs_app_get_metadata_item (app, "XgdApp::type"), "runtime") == 0)
		args[1] = "install-runtime";

	/* run tool sync */
	results = gs_plugin_xdg_app_exec (args, error);
	if (results == NULL)
		return FALSE;
	return TRUE;
}
