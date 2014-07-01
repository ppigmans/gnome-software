/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Matthias Clasen <mclasen@redhat.com>
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

/**
 * SECTION:gs-app
 * @short_description: An application that is either installed or that can be installed
 *
 * This object represents a 1:1 mapping to a .desktop file. The design is such
 * so you can't have different AsApp's for different versions or architectures
 * of a package. This rule really only applies to GsApps of kind GS_APP_KIND_NORMAL
 * and GS_APP_KIND_SYSTEM. We allow GsApps of kind GS_APP_KIND_SYSTEM_UPDATE or
 * GS_APP_KIND_PACKAGE, which don't correspond to desktop files, but instead
 * represent a system update and its individual components.
 *
 * The #GsPluginLoader de-duplicates the AsApp instances that are produced by
 * plugins to ensure that there is a single instance of AsApp for each id, making
 * the id the primary key for this object. This ensures that actions triggered on
 * a AsApp in different parts of gnome-software can be observed by connecting to
 * signals on the AsApp.
 *
 * Information about other #AsApp objects can be stored in this object, for
 * instance in the gs_app_add_related() method or gs_app_get_history().
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>

#include "gs-app.h"
#include "gs-utils.h"

static void	gs_app_finalize	(GObject	*object);

struct GsAppPrivate
{
	GsAppQuality		 name_quality;
	GPtrArray		*sources;
	GPtrArray		*source_ids;
	gchar			*version;
	gchar			*version_ui;
	GsAppQuality		 summary_quality;
	gchar			*summary_missing;
	GsAppQuality		 description_quality;
	GPtrArray		*keywords;
	gchar			*menu_path;
	gchar			*origin;
	gchar			*update_version;
	gchar			*update_version_ui;
	gchar			*update_details;
	gchar			*management_plugin;
	gint			 rating;
	gint			 rating_confidence;
	GsAppRatingKind		 rating_kind;
	guint64			 size;
	GsAppKind		 kind;
	GdkPixbuf		*pixbuf;
	GdkPixbuf		*featured_pixbuf;
	GPtrArray		*addons; /* of AsApp */
	GHashTable		*addons_hash; /* of "id" */
	GPtrArray		*related; /* of AsApp */
	GHashTable		*related_hash; /* of "id-source" */
	GPtrArray		*history; /* of AsApp */
	guint64			 install_date;
	guint64			 kudos;
	gboolean		 to_be_installed;
};

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_VERSION,
	PROP_SUMMARY,
	PROP_DESCRIPTION,
	PROP_RATING,
	PROP_KIND,
	PROP_STATE,
	PROP_INSTALL_DATE,
	PROP_LAST
};

G_DEFINE_TYPE_WITH_PRIVATE (GsApp, gs_app, AS_TYPE_APP)

/**
 * gs_app_kind_to_string:
 **/
const gchar *
gs_app_kind_to_string (GsAppKind kind)
{
	if (kind == GS_APP_KIND_UNKNOWN)
		return "unknown";
	if (kind == GS_APP_KIND_NORMAL)
		return "normal";
	if (kind == GS_APP_KIND_SYSTEM)
		return "system";
	if (kind == GS_APP_KIND_PACKAGE)
		return "package";
	if (kind == GS_APP_KIND_OS_UPDATE)
		return "os-update";
	if (kind == GS_APP_KIND_MISSING)
		return "missing";
	if (kind == GS_APP_KIND_SOURCE)
		return "source";
	if (kind == GS_APP_KIND_CORE)
		return "core";
	return NULL;
}

/**
 * gs_app_to_string:
 **/
gchar *
gs_app_to_string (AsApp *app)
{
	AsImage *im;
	AsScreenshot *ss;
	GList *keys;
	GList *l;
	GString *str;
	GPtrArray *categories;
	GPtrArray *screenshots;
	GsAppPrivate *priv = GS_APP(app)->priv;
	const gchar *tmp;
	guint i;

	g_return_val_if_fail (GS_IS_APP (app), NULL);

	str = g_string_new ("AsApp:\n");
	g_string_append_printf (str, "\tkind:\t%s\n",
				gs_app_kind_to_string (priv->kind));
	if (as_app_get_id_kind (app) != AS_ID_KIND_UNKNOWN) {
		g_string_append_printf (str, "\tid-kind:\t%s\n",
					as_id_kind_to_string (as_app_get_id_kind (app)));
	}
	g_string_append_printf (str, "\tstate:\t%s\n",
				as_app_state_to_string (as_app_get_state (app)));
	if (as_app_get_id_full (app) != NULL)
		g_string_append_printf (str, "\tid:\t%s\n", as_app_get_id_full (app));
	if ((priv->kudos & GS_APP_KUDO_MY_LANGUAGE) > 0)
		g_string_append (str, "\tkudo:\tmy-language\n");
	if ((priv->kudos & GS_APP_KUDO_RECENT_RELEASE) > 0)
		g_string_append (str, "\tkudo:\trecent-release\n");
	if ((priv->kudos & GS_APP_KUDO_FEATURED_RECOMMENDED) > 0)
		g_string_append (str, "\tkudo:\tfeatured-recommended\n");
	if ((priv->kudos & GS_APP_KUDO_MODERN_TOOLKIT) > 0)
		g_string_append (str, "\tkudo:\tmodern-toolkit\n");
	if ((priv->kudos & GS_APP_KUDO_SEARCH_PROVIDER) > 0)
		g_string_append (str, "\tkudo:\tsearch-provider\n");
	if ((priv->kudos & GS_APP_KUDO_INSTALLS_USER_DOCS) > 0)
		g_string_append (str, "\tkudo:\tinstalls-user-docs\n");
	if ((priv->kudos & GS_APP_KUDO_USES_NOTIFICATIONS) > 0)
		g_string_append (str, "\tkudo:\tuses-notifications\n");
	if ((priv->kudos & GS_APP_KUDO_USES_APP_MENU) > 0)
		g_string_append (str, "\tkudo:\tuses-app-menu\n");
	if ((priv->kudos & GS_APP_KUDO_HAS_KEYWORDS) > 0)
		g_string_append (str, "\tkudo:\thas-keywords\n");
	if ((priv->kudos & GS_APP_KUDO_HAS_SCREENSHOTS) > 0)
		g_string_append (str, "\tkudo:\thas-screenshots\n");
	if ((priv->kudos & GS_APP_KUDO_POPULAR) > 0)
		g_string_append (str, "\tkudo:\tpopular\n");
	if ((priv->kudos & GS_APP_KUDO_IBUS_HAS_SYMBOL) > 0)
		g_string_append (str, "\tkudo:\tibus-has-symbol\n");
	g_string_append_printf (str, "\tkudo-percentage:\t%i\n",
				gs_app_get_kudos_percentage (app));
	if (as_app_get_name (app, NULL) != NULL)
		g_string_append_printf (str, "\tname:\t%s\n", as_app_get_name (app, NULL));
	if (as_app_get_icon (app) != NULL)
		g_string_append_printf (str, "\ticon:\t%s\n", as_app_get_icon (app));
	if (priv->version != NULL)
		g_string_append_printf (str, "\tversion:\t%s\n", priv->version);
	if (priv->version_ui != NULL)
		g_string_append_printf (str, "\tversion-ui:\t%s\n", priv->version_ui);
	if (priv->update_version != NULL)
		g_string_append_printf (str, "\tupdate-version:\t%s\n", priv->update_version);
	if (priv->update_version_ui != NULL)
		g_string_append_printf (str, "\tupdate-version-ui:\t%s\n", priv->update_version_ui);
	if (priv->update_details != NULL) {
		g_string_append_printf (str, "\tupdate-details:\t%s\n",
					priv->update_details);
	}
	if (as_app_get_comment (app, NULL) != NULL)
		g_string_append_printf (str, "\tsummary:\t%s\n", as_app_get_comment (app, NULL));
	if (as_app_get_description (app, NULL) != NULL)
		g_string_append_printf (str, "\tdescription:\t%s\n", as_app_get_description (app, NULL));
	screenshots = as_app_get_screenshots (app);
	for (i = 0; i < screenshots->len; i++) {
		ss = g_ptr_array_index (screenshots, i);
		tmp = as_screenshot_get_caption (ss, NULL);
		im = as_screenshot_get_image (ss, 0, 0);
		if (im == NULL)
			continue;
		g_string_append_printf (str, "\tscreenshot-%02i:\t%s [%s]\n",
					i, as_image_get_url (im),
					tmp != NULL ? tmp : "<none>");
	}
	for (i = 0; i < priv->sources->len; i++) {
		tmp = g_ptr_array_index (priv->sources, i);
		g_string_append_printf (str, "\tsource-%02i:\t%s\n", i, tmp);
	}
	for (i = 0; i < priv->source_ids->len; i++) {
		tmp = g_ptr_array_index (priv->source_ids, i);
		g_string_append_printf (str, "\tsource-id-%02i:\t%s\n", i, tmp);
	}
	tmp = as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE);
	if (tmp != NULL)
		g_string_append_printf (str, "\turl{homepage}:\t%s\n", tmp);
	if (as_app_get_project_license (app) != NULL)
		g_string_append_printf (str, "\tlicence:\t%s\n", as_app_get_project_license (app));
	if (priv->summary_missing != NULL)
		g_string_append_printf (str, "\tsummary-missing:\t%s\n", priv->summary_missing);
	if (priv->menu_path != NULL && priv->menu_path[0] != '\0')
		g_string_append_printf (str, "\tmenu-path:\t%s\n", priv->menu_path);
	if (priv->origin != NULL && priv->origin[0] != '\0')
		g_string_append_printf (str, "\torigin:\t%s\n", priv->origin);
	if (priv->rating != -1)
		g_string_append_printf (str, "\trating:\t%i\n", priv->rating);
	if (priv->rating_confidence != -1)
		g_string_append_printf (str, "\trating-confidence:\t%i\n", priv->rating_confidence);
	if (priv->rating_kind != GS_APP_RATING_KIND_UNKNOWN)
		g_string_append_printf (str, "\trating-kind:\t%s\n",
					priv->rating_kind == GS_APP_RATING_KIND_USER ?
						"user" : "system");
	if (priv->pixbuf != NULL)
		g_string_append_printf (str, "\tpixbuf:\t%p\n", priv->pixbuf);
	if (priv->featured_pixbuf != NULL)
		g_string_append_printf (str, "\tfeatured-pixbuf:\t%p\n", priv->featured_pixbuf);
	if (priv->install_date != 0) {
		g_string_append_printf (str, "\tinstall-date:\t%"
					G_GUINT64_FORMAT "\n",
					priv->install_date);
	}
	if (priv->size != 0) {
		g_string_append_printf (str, "\tsize:\t%" G_GUINT64_FORMAT "k\n",
					priv->size / 1024);
	}
	if (priv->related->len > 0)
		g_string_append_printf (str, "\trelated:\t%i\n", priv->related->len);
	if (priv->history->len > 0)
		g_string_append_printf (str, "\thistory:\t%i\n", priv->history->len);
	categories = as_app_get_categories (app);
	for (i = 0; i < categories->len; i++) {
		tmp = g_ptr_array_index (categories, i);
		g_string_append_printf (str, "\tcategory:\t%s\n", tmp);
	}
	keys = g_hash_table_get_keys (as_app_get_metadata (app));
	for (l = keys; l != NULL; l = l->next) {
		tmp = g_hash_table_lookup (as_app_get_metadata (app), l->data);
		g_string_append_printf (str, "\t{%s}:\t%s\n",
					(const gchar *) l->data, tmp);
	}
	g_list_free (keys);
	return g_string_free (str, FALSE);
}

typedef struct {
	AsApp *app;
	gchar *property_name;
} AppNotifyData;

static gboolean
notify_idle_cb (gpointer data)
{
	AppNotifyData *notify_data = data;

	g_object_notify (G_OBJECT (notify_data->app),
	                 notify_data->property_name);

	g_object_unref (notify_data->app);
	g_free (notify_data->property_name);
	g_free (notify_data);

	return G_SOURCE_REMOVE;
}

static void
gs_app_queue_notify (AsApp *app, const gchar *property_name)
{
	AppNotifyData *notify_data;
	guint id;

	notify_data = g_new (AppNotifyData, 1);
	notify_data->app = g_object_ref (app);
	notify_data->property_name = g_strdup (property_name);

	id = g_idle_add (notify_idle_cb, notify_data);
	g_source_set_name_by_id (id, "[gnome-software] notify_idle_cb");
}

/**
 * gs_app_set_state_internal:
 */
static gboolean
gs_app_set_state_internal (AsApp *app, AsAppState state)
{
	gboolean state_change_ok = FALSE;

	if (as_app_get_state (app) == state)
		return FALSE;

	/* check the state change is allowed */
	switch (as_app_get_state (app)) {
	case AS_APP_STATE_UNKNOWN:
		/* unknown has to go into one of the stable states */
		if (state == AS_APP_STATE_INSTALLED ||
		    state == AS_APP_STATE_QUEUED_FOR_INSTALL ||
		    state == AS_APP_STATE_AVAILABLE ||
		    state == AS_APP_STATE_AVAILABLE_LOCAL ||
		    state == AS_APP_STATE_UPDATABLE ||
		    state == AS_APP_STATE_UNAVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_INSTALLED:
		/* installed has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_REMOVING)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_QUEUED_FOR_INSTALL:
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_INSTALLING ||
		    state == AS_APP_STATE_AVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_AVAILABLE:
		/* available has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_QUEUED_FOR_INSTALL ||
		    state == AS_APP_STATE_INSTALLING)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_INSTALLING:
		/* installing has to go into an stable state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_INSTALLED ||
		    state == AS_APP_STATE_AVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_REMOVING:
		/* removing has to go into an stable state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_AVAILABLE ||
		    state == AS_APP_STATE_INSTALLED)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_UPDATABLE:
		/* updatable has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_REMOVING)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_UNAVAILABLE:
		/* updatable has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_AVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_AVAILABLE_LOCAL:
		/* local has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_INSTALLING)
			state_change_ok = TRUE;
		break;
	default:
		g_warning ("state %s unhandled",
			   as_app_state_to_string (as_app_get_state (app)));
		g_assert_not_reached ();
	}

	/* this state change was unexpected */
	if (!state_change_ok) {
		g_warning ("State change on %s from %s to %s is not OK",
			   as_app_get_id (app),
			   as_app_state_to_string (as_app_get_state (app)),
			   as_app_state_to_string (state));
		return FALSE;
	}

	as_app_set_state (app, state);

	if (state == AS_APP_STATE_UNKNOWN ||
	    state == AS_APP_STATE_AVAILABLE_LOCAL ||
	    state == AS_APP_STATE_AVAILABLE)
		GS_APP(app)->priv->install_date = 0;

	return TRUE;
}

/**
 * gs_app_set_state:
 *
 * This sets the state of the application. The following state diagram explains
 * the typical states. All applications start in state %AS_APP_STATE_UNKNOWN,
 * but the frontend is not supposed to see GsApps with this state, ever.
 * Backend plugins are reponsible for changing the state to one of the other
 * states before the AsApp is passed to the frontend. This is enforced by the
 * #GsPluginLoader.
 *
 * UPDATABLE --> INSTALLING --> INSTALLED
 * UPDATABLE --> REMOVING   --> AVAILABLE
 * INSTALLED --> REMOVING   --> AVAILABLE
 * AVAILABLE --> INSTALLING --> INSTALLED
 * AVAILABLE <--> QUEUED --> INSTALLING --> INSTALLED
 * UNKNOWN   --> UNAVAILABLE
 */
void
gs_app_set_state (AsApp *app, AsAppState state)
{
	g_return_if_fail (GS_IS_APP (app));

	if (gs_app_set_state_internal (app, state))
		gs_app_queue_notify (app, "state");
}

/**
 * gs_app_get_kind:
 */
GsAppKind
gs_app_get_kind (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), GS_APP_KIND_UNKNOWN);
	return GS_APP(app)->priv->kind;
}

/**
 * gs_app_set_kind:
 *
 * This sets the kind of the application. The following state diagram explains
 * the typical states. All applications start with kind %GS_APP_KIND_UNKNOWN.
 *
 * PACKAGE --> NORMAL
 * PACKAGE --> SYSTEM
 * NORMAL  --> SYSTEM
 */
void
gs_app_set_kind (AsApp *app, GsAppKind kind)
{
	gboolean state_change_ok = FALSE;
	GsAppPrivate *priv = GS_APP(app)->priv;

	g_return_if_fail (GS_IS_APP (app));
	if (priv->kind == kind)
		return;

	/* check the state change is allowed */
	switch (priv->kind) {
	case GS_APP_KIND_UNKNOWN:
		/* unknown can go into any state */
		state_change_ok = TRUE;
		break;
	case GS_APP_KIND_PACKAGE:
		/* package can become either normal or a system application */
		if (kind == GS_APP_KIND_NORMAL ||
		    kind == GS_APP_KIND_SYSTEM ||
		    kind == GS_APP_KIND_CORE ||
		    kind == GS_APP_KIND_SOURCE ||
		    kind == GS_APP_KIND_UNKNOWN)
			state_change_ok = TRUE;
		break;
	case GS_APP_KIND_NORMAL:
		/* normal can only be promoted to system */
		if (kind == GS_APP_KIND_SYSTEM ||
		    kind == GS_APP_KIND_UNKNOWN)
			state_change_ok = TRUE;
		break;
	case GS_APP_KIND_SYSTEM:
	case GS_APP_KIND_OS_UPDATE:
	case GS_APP_KIND_SOURCE:
	case GS_APP_KIND_MISSING:
		/* this can never change state */
		break;
	default:
		g_warning ("kind %s unhandled",
			   gs_app_kind_to_string (priv->kind));
		g_assert_not_reached ();
	}

	/* this state change was unexpected */
	if (!state_change_ok) {
		g_warning ("Kind change on %s from %s to %s is not OK",
			   as_app_get_id (app),
			   gs_app_kind_to_string (priv->kind),
			   gs_app_kind_to_string (kind));
		return;
	}

	priv->kind = kind;
	gs_app_queue_notify (app, "kind");
}

/**
 * gs_app_set_name:
 * @app:	A #AsApp instance
 * @quality:	A data quality, e.g. %GS_APP_QUALITY_LOWEST
 * @name:	The short localized name, e.g. "Calculator"
 */
void
gs_app_set_name (AsApp *app, GsAppQuality quality, const gchar *name)
{
	g_return_if_fail (GS_IS_APP (app));

	/* only save this if the data is sufficiently high quality */
	if (quality < GS_APP(app)->priv->name_quality)
		return;
	GS_APP(app)->priv->name_quality = quality;
	as_app_set_name (app, NULL, name, -1);
}

/**
 * gs_app_get_source_default:
 */
const gchar *
gs_app_get_source_default (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	if (GS_APP(app)->priv->sources->len == 0)
		return NULL;
	return g_ptr_array_index (GS_APP(app)->priv->sources, 0);
}

/**
 * gs_app_add_source:
 */
void
gs_app_add_source (AsApp *app, const gchar *source)
{
	const gchar *tmp;
	guint i;

	g_return_if_fail (GS_IS_APP (app));

	/* check source doesn't already exist */
	for (i = 0; i < GS_APP(app)->priv->sources->len; i++) {
		tmp = g_ptr_array_index (GS_APP(app)->priv->sources, i);
		if (g_strcmp0 (tmp, source) == 0)
			return;
	}
	g_ptr_array_add (GS_APP(app)->priv->sources, g_strdup (source));
}

/**
 * gs_app_get_sources:
 */
GPtrArray *
gs_app_get_sources (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->sources;
}

/**
 * gs_app_set_sources:
 * @app:	A #AsApp instance
 * @source:	The non-localized short names, e.g. ["gnome-calculator"]
 *
 * This name is used for the update page if the application is collected into
 * the 'OS Updates' group. It is typically the package names, although this
 * should not be relied upon.
 */
void
gs_app_set_sources (AsApp *app, GPtrArray *sources)
{
	g_return_if_fail (GS_IS_APP (app));
	if (GS_APP(app)->priv->sources != NULL)
		g_ptr_array_unref (GS_APP(app)->priv->sources);
	GS_APP(app)->priv->sources = g_ptr_array_ref (sources);
}

/**
 * gs_app_get_source_id_default:
 */
const gchar *
gs_app_get_source_id_default (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	if (GS_APP(app)->priv->source_ids->len == 0)
		return NULL;
	return g_ptr_array_index (GS_APP(app)->priv->source_ids, 0);
}

/**
 * gs_app_get_source_ids:
 */
GPtrArray *
gs_app_get_source_ids (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->source_ids;
}

/**
 * gs_app_set_source_ids:
 * @app:	A #AsApp instance
 * @source_id:	The source-id, e.g. ["gnome-calculator;0.134;fedora"]
 * 		or ["/home/hughsie/.local/share/applications/0ad.desktop"]
 *
 * This ID is used internally to the controlling plugin.
 */
void
gs_app_set_source_ids (AsApp *app, GPtrArray *source_ids)
{
	g_return_if_fail (GS_IS_APP (app));
	if (GS_APP(app)->priv->source_ids != NULL)
		g_ptr_array_unref (GS_APP(app)->priv->source_ids);
	GS_APP(app)->priv->source_ids = g_ptr_array_ref (source_ids);
}

/**
 * gs_app_add_source_id:
 */
void
gs_app_add_source_id (AsApp *app, const gchar *source_id)
{
	const gchar *tmp;
	guint i;

	g_return_if_fail (GS_IS_APP (app));

	/* only add if not already present */
	for (i = 0; i < GS_APP(app)->priv->source_ids->len; i++) {
		tmp = g_ptr_array_index (GS_APP(app)->priv->source_ids, i);
		if (g_strcmp0 (tmp, source_id) == 0)
			return;
	}
	g_ptr_array_add (GS_APP(app)->priv->source_ids, g_strdup (source_id));
}

/**
 * gs_app_get_pixbuf:
 */
GdkPixbuf *
gs_app_get_pixbuf (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->pixbuf;
}

/**
 * gs_app_load_icon:
 */
gboolean
gs_app_load_icon (AsApp *app, GError **error)
{
	GdkPixbuf *pixbuf = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GS_IS_APP (app), FALSE);

	/* either load from the theme or from a file */
	pixbuf = gs_pixbuf_load (as_app_get_icon (app), 64, error);
	if (pixbuf == NULL) {
		ret = FALSE;
		goto out;
	}
	gs_app_set_pixbuf (app, pixbuf);
out:
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
	return ret;
}

/**
 * gs_app_set_pixbuf:
 */
void
gs_app_set_pixbuf (AsApp *app, GdkPixbuf *pixbuf)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
	if (GS_APP(app)->priv->pixbuf != NULL)
		g_object_unref (GS_APP(app)->priv->pixbuf);
	GS_APP(app)->priv->pixbuf = g_object_ref (pixbuf);
}

/**
 * gs_app_get_featured_pixbuf:
 */
GdkPixbuf *
gs_app_get_featured_pixbuf (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->featured_pixbuf;
}

/**
 * gs_app_set_featured_pixbuf:
 */
void
gs_app_set_featured_pixbuf (AsApp *app, GdkPixbuf *pixbuf)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (GS_APP(app)->priv->featured_pixbuf == NULL);
	GS_APP(app)->priv->featured_pixbuf = g_object_ref (pixbuf);
}

typedef enum {
	GS_APP_VERSION_FIXUP_RELEASE		= 1,
	GS_APP_VERSION_FIXUP_DISTRO_SUFFIX	= 2,
	GS_APP_VERSION_FIXUP_GIT_SUFFIX		= 4,
	GS_APP_VERSION_FIXUP_LAST,
} GsAppVersionFixup;

/**
 * gs_app_get_ui_version:
 *
 * convert 1:1.6.2-7.fc17 into "Version 1.6.2"
 **/
static gchar *
gs_app_get_ui_version (const gchar *version, guint64 flags)
{
	guint i;
	gchar *new = NULL;
	gchar *f;

	/* nothing set */
	if (version == NULL)
		goto out;

	/* first remove any epoch */
	for (i = 0; version[i] != '\0'; i++) {
		if (version[i] == ':') {
			version = &version[i+1];
			break;
		}
		if (!g_ascii_isdigit (version[i]))
			break;
	}

	/* then remove any distro suffix */
	new = g_strdup (version);
	if ((flags & GS_APP_VERSION_FIXUP_DISTRO_SUFFIX) > 0) {
		f = g_strstr_len (new, -1, ".fc");
		if (f != NULL)
			*f= '\0';
	}

	/* then remove any release */
	if ((flags & GS_APP_VERSION_FIXUP_RELEASE) > 0) {
		f = g_strrstr_len (new, -1, "-");
		if (f != NULL)
			*f= '\0';
	}

	/* then remove any git suffix */
	if ((flags & GS_APP_VERSION_FIXUP_GIT_SUFFIX) > 0) {
		f = g_strrstr_len (new, -1, ".2012");
		if (f != NULL)
			*f= '\0';
		f = g_strrstr_len (new, -1, ".2013");
		if (f != NULL)
			*f= '\0';
	}
out:
	return new;
}

/**
 * gs_app_ui_versions_invalidate:
 */
static void
gs_app_ui_versions_invalidate (AsApp *app)
{
	GsAppPrivate *priv = GS_APP(app)->priv;
	g_free (priv->version_ui);
	g_free (priv->update_version_ui);
	priv->version_ui = NULL;
	priv->update_version_ui = NULL;
}

/**
 * gs_app_ui_versions_populate:
 */
static void
gs_app_ui_versions_populate (AsApp *app)
{
	GsAppPrivate *priv = GS_APP(app)->priv;
	guint i;
	guint64 flags[] = { GS_APP_VERSION_FIXUP_RELEASE |
			    GS_APP_VERSION_FIXUP_DISTRO_SUFFIX |
			    GS_APP_VERSION_FIXUP_GIT_SUFFIX,
			    GS_APP_VERSION_FIXUP_DISTRO_SUFFIX |
			    GS_APP_VERSION_FIXUP_GIT_SUFFIX,
			    GS_APP_VERSION_FIXUP_DISTRO_SUFFIX,
			    0 };

	/* try each set of bitfields in order */
	for (i = 0; flags[i] != 0; i++) {
		priv->version_ui = gs_app_get_ui_version (priv->version, flags[i]);
		priv->update_version_ui = gs_app_get_ui_version (priv->update_version, flags[i]);
		if (g_strcmp0 (priv->version_ui, priv->update_version_ui) != 0) {
			gs_app_queue_notify (app, "version");
			return;
		}
		gs_app_ui_versions_invalidate (app);
	}

	/* we tried, but failed */
	priv->version_ui = g_strdup (priv->version);
	priv->update_version_ui = g_strdup (priv->update_version);
}

/**
 * gs_app_get_version:
 */
const gchar *
gs_app_get_version (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->version;
}

/**
 * gs_app_get_version_ui:
 */
const gchar *
gs_app_get_version_ui (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);

	/* work out the two version numbers */
	if (GS_APP(app)->priv->version != NULL &&
	    GS_APP(app)->priv->version_ui == NULL) {
		gs_app_ui_versions_populate (app);
	}

	return GS_APP(app)->priv->version_ui;
}

/**
 * gs_app_set_version:
 * @app:	A #AsApp instance
 * @version:	The version, e.g. "2:1.2.3.fc19"
 *
 * This saves the version after stripping out any non-friendly parts, such as
 * distro tags, git revisions and that kind of thing.
 */
void
gs_app_set_version (AsApp *app, const gchar *version)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (GS_APP(app)->priv->version);
	GS_APP(app)->priv->version = g_strdup (version);
	gs_app_ui_versions_invalidate (app);
	gs_app_queue_notify (app, "version");
}

/**
 * gs_app_set_summary:
 * @app:	A #AsApp instance
 * @quality:	A data quality, e.g. %GS_APP_QUALITY_LOWEST
 * @summary:	The medium length localized name, e.g. "A graphical calculator for GNOME"
 */
void
gs_app_set_summary (AsApp *app, GsAppQuality quality, const gchar *summary)
{
	g_return_if_fail (GS_IS_APP (app));

	/* only save this if the data is sufficiently high quality */
	if (quality < GS_APP(app)->priv->summary_quality)
		return;
	GS_APP(app)->priv->summary_quality = quality;
	as_app_set_comment (app, NULL, summary, -1);
}

/**
 * gs_app_set_description:
 * @app:	A #AsApp instance
 * @quality:	A data quality, e.g. %GS_APP_QUALITY_LOWEST
 * @summary:	The multiline localized description, e.g. "GNOME Calculator is a graphical calculator for GNOME....."
 */
void
gs_app_set_description (AsApp *app, GsAppQuality quality, const gchar *description)
{
	g_return_if_fail (GS_IS_APP (app));

	/* only save this if the data is sufficiently high quality */
	if (quality < GS_APP(app)->priv->description_quality)
		return;
	GS_APP(app)->priv->description_quality = quality;
	as_app_set_description (app, NULL, description, -1);
}

/**
 * gs_app_get_summary_missing:
 */
const gchar *
gs_app_get_summary_missing (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->summary_missing;
}

/**
 * gs_app_set_summary_missing:
 */
void
gs_app_set_summary_missing (AsApp *app, const gchar *summary_missing)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (GS_APP(app)->priv->summary_missing);
	GS_APP(app)->priv->summary_missing = g_strdup (summary_missing);
}

/**
 * gs_app_get_menu_path:
 */
const gchar *
gs_app_get_menu_path (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->menu_path;
}

/**
 * gs_app_set_menu_path:
 */
void
gs_app_set_menu_path (AsApp *app, const gchar *menu_path)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (GS_APP(app)->priv->menu_path);
	GS_APP(app)->priv->menu_path = g_strdup (menu_path);
}

/**
 * gs_app_get_origin:
 */
const gchar *
gs_app_get_origin (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->origin;
}

/**
 * gs_app_set_origin:
 *
 * The origin is the original source of the application to show in the UI,
 * e.g. "Fedora"
 */
void
gs_app_set_origin (AsApp *app, const gchar *origin)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (GS_APP(app)->priv->origin);
	GS_APP(app)->priv->origin = g_strdup (origin);
}

/**
 * gs_app_get_update_version:
 */
const gchar *
gs_app_get_update_version (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->update_version;
}

/**
 * gs_app_get_update_version_ui:
 */
const gchar *
gs_app_get_update_version_ui (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);

	/* work out the two version numbers */
	if (GS_APP(app)->priv->update_version != NULL &&
	    GS_APP(app)->priv->update_version_ui == NULL) {
		gs_app_ui_versions_populate (app);
	}

	return GS_APP(app)->priv->update_version_ui;
}

/**
 * gs_app_set_update_version_internal:
 */
static void
gs_app_set_update_version_internal (AsApp *app, const gchar *update_version)
{
	g_free (GS_APP(app)->priv->update_version);
	GS_APP(app)->priv->update_version = g_strdup (update_version);
	gs_app_ui_versions_invalidate (app);
}

/**
 * gs_app_set_update_version:
 */
void
gs_app_set_update_version (AsApp *app, const gchar *update_version)
{
	g_return_if_fail (GS_IS_APP (app));
	gs_app_set_update_version_internal (app, update_version);
	gs_app_queue_notify (app, "version");
}

/**
 * gs_app_get_update_details:
 */
const gchar *
gs_app_get_update_details (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->update_details;
}

/**
 * gs_app_set_update_details:
 */
void
gs_app_set_update_details (AsApp *app, const gchar *update_details)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (GS_APP(app)->priv->update_details);
	GS_APP(app)->priv->update_details = g_strdup (update_details);
}

/**
 * gs_app_get_management_plugin:
 */
const gchar *
gs_app_get_management_plugin (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->management_plugin;
}

/**
 * gs_app_set_management_plugin:
 *
 * The management plugin is the plugin that can handle doing install and remove
 * operations on the #AsApp. Typical values include "PackageKit" and "jhbuild"
 */
void
gs_app_set_management_plugin (AsApp *app, const gchar *management_plugin)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (GS_APP(app)->priv->management_plugin);
	GS_APP(app)->priv->management_plugin = g_strdup (management_plugin);
}

/**
 * gs_app_get_rating:
 */
gint
gs_app_get_rating (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), -1);
	return GS_APP(app)->priv->rating;
}

/**
 * gs_app_set_rating:
 */
void
gs_app_set_rating (AsApp *app, gint rating)
{
	g_return_if_fail (GS_IS_APP (app));
	GS_APP(app)->priv->rating = rating;
	gs_app_queue_notify (app, "rating");
}

/**
 * gs_app_get_rating_confidence:
 * @app:	A #AsApp instance
 *
 * Return value: a predictor from 0 to 100, or -1 for unknown or invalid
 */
gint
gs_app_get_rating_confidence (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), -1);
	return GS_APP(app)->priv->rating_confidence;
}

/**
 * gs_app_set_rating_confidence:
 * @app:	A #AsApp instance
 * @rating_confidence:	a predictor from 0 to 100, or -1 for unknown or invalid
 *
 * This is how confident the rating is statistically valid, expressed as a
 * percentage.
 * Applications with a high confidence typically have a large number of samples
 * and can be trusted, but low confidence could mean that only one person has
 * rated the application.
 */
void
gs_app_set_rating_confidence (AsApp *app, gint rating_confidence)
{
	g_return_if_fail (GS_IS_APP (app));
	GS_APP(app)->priv->rating_confidence = rating_confidence;
}

/**
 * gs_app_get_rating_kind:
 */
GsAppRatingKind
gs_app_get_rating_kind (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), -1);
	return GS_APP(app)->priv->rating_kind;
}

/**
 * gs_app_set_rating_kind:
 */
void
gs_app_set_rating_kind (AsApp *app, GsAppRatingKind rating_kind)
{
	g_return_if_fail (GS_IS_APP (app));
	GS_APP(app)->priv->rating_kind = rating_kind;
	gs_app_queue_notify (app, "rating");
}

/**
 * gs_app_get_size:
 */
guint64
gs_app_get_size (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), G_MAXUINT64);
	return GS_APP(app)->priv->size;
}

/**
 * gs_app_set_size:
 */
void
gs_app_set_size (AsApp *app, guint64 size)
{
	g_return_if_fail (GS_IS_APP (app));
	GS_APP(app)->priv->size = size;
}

/**
 * gs_app_get_addons:
 */
GPtrArray *
gs_app_get_addons (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->addons;
}

/**
 * gs_app_add_addon:
 */
void
gs_app_add_addon (AsApp *app, AsApp *addon)
{
	gpointer found;
	const gchar *id;

	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (GS_IS_APP (addon));

	id = as_app_get_id (addon);
	found = g_hash_table_lookup (GS_APP(app)->priv->addons_hash, id);
	if (found != NULL) {
		g_debug ("Already added %s as an addon", id);
		return;
	}
	g_hash_table_insert (GS_APP(app)->priv->addons_hash, g_strdup (id), GINT_TO_POINTER (1));

	g_ptr_array_add (GS_APP(app)->priv->addons, g_object_ref (addon));
}

/**
 * gs_app_get_related:
 */
GPtrArray *
gs_app_get_related (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->related;
}

/**
 * gs_app_add_related:
 */
void
gs_app_add_related (AsApp *app, AsApp *app2)
{
	gchar *key;
	gpointer found;

	g_return_if_fail (GS_IS_APP (app));

	key = g_strdup_printf ("%s-%s",
			       as_app_get_id_full (app2),
			       gs_app_get_source_default (app2));
	found = g_hash_table_lookup (GS_APP(app)->priv->related_hash, key);
	if (found != NULL) {
		g_debug ("Already added %s as a related item", key);
		g_free (key);
		return;
	}
	g_hash_table_insert (GS_APP(app)->priv->related_hash, key, GINT_TO_POINTER (1));
	g_ptr_array_add (GS_APP(app)->priv->related, g_object_ref (app2));
}

/**
 * gs_app_get_history:
 */
GPtrArray *
gs_app_get_history (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return GS_APP(app)->priv->history;
}

/**
 * gs_app_add_history:
 */
void
gs_app_add_history (AsApp *app, AsApp *app2)
{
	g_return_if_fail (GS_IS_APP (app));
	g_ptr_array_add (GS_APP(app)->priv->history, g_object_ref (app2));
}

guint64
gs_app_get_install_date (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), 0);
	return GS_APP(app)->priv->install_date;
}

void
gs_app_set_install_date (AsApp *app, guint64 install_date)
{
	g_return_if_fail (GS_IS_APP (app));
	GS_APP(app)->priv->install_date = install_date;
}

/**
 * gs_app_add_kudo:
 */
void
gs_app_add_kudo (AsApp *app, GsAppKudo kudo)
{
	g_return_if_fail (GS_IS_APP (app));
	GS_APP(app)->priv->kudos |= kudo;
}

/**
 * gs_app_get_kudos:
 */
guint64
gs_app_get_kudos (AsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), 0);
	return GS_APP(app)->priv->kudos;
}

/**
 * gs_app_get_kudos_weight:
 */
guint
gs_app_get_kudos_weight (AsApp *app)
{
	guint32 tmp = GS_APP(app)->priv->kudos;
	tmp = tmp - ((tmp >> 1) & 0x55555555);
	tmp = (tmp & 0x33333333) + ((tmp >> 2) & 0x33333333);
	return (((tmp + (tmp >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/**
 * gs_app_get_kudos_percentage:
 */
guint
gs_app_get_kudos_percentage (AsApp *app)
{
	guint percentage = 0;

	g_return_val_if_fail (GS_IS_APP (app), 0);

	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_MY_LANGUAGE) > 0)
		percentage += 20;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_RECENT_RELEASE) > 0)
		percentage += 20;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_FEATURED_RECOMMENDED) > 0)
		percentage += 20;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_MODERN_TOOLKIT) > 0)
		percentage += 20;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_SEARCH_PROVIDER) > 0)
		percentage += 10;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_INSTALLS_USER_DOCS) > 0)
		percentage += 10;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_USES_NOTIFICATIONS) > 0)
		percentage += 20;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_HAS_KEYWORDS) > 0)
		percentage += 5;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_USES_APP_MENU) > 0)
		percentage += 10;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_HAS_SCREENSHOTS) > 0)
		percentage += 20;
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_IBUS_HAS_SYMBOL) > 0)
		percentage += 20;

	/* popular apps should be at *least* 50% */
	if ((GS_APP(app)->priv->kudos & GS_APP_KUDO_POPULAR) > 0)
		percentage = MAX (percentage, 50);

	return MIN (percentage, 100);
}

/**
 * gs_app_get_to_be_installed:
 */
gboolean
gs_app_get_to_be_installed (AsApp *app)
{
	GsAppPrivate *priv = GS_APP(app)->priv;
	return priv->to_be_installed;
}

/**
 * gs_app_set_to_be_installed:
 */
void
gs_app_set_to_be_installed (AsApp *app, gboolean to_be_installed)
{
	GsAppPrivate *priv = GS_APP(app)->priv;
	priv->to_be_installed = to_be_installed;
}

/**
 * gs_app_subsume:
 *
 * Imports all the useful data from @other into @app.
 *
 * IMPORTANT: This method can be called from a thread as the notify signals
 * are not sent.
 **/
void
gs_app_subsume (AsApp *app, AsApp *other)
{
//	const gchar *tmp;
//	GList *keys;
//	GList *l;
	AsApp *app_tmp;
	GsAppPrivate *priv2 = GS_APP(other)->priv;
	GsAppPrivate *priv = GS_APP(app)->priv;
	guint i;

	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (GS_IS_APP (other));

	/* an [updatable] installable package is more information than
	 * just the fact that something is installed */
	if (as_app_get_state (other) == AS_APP_STATE_UPDATABLE &&
	    as_app_get_state (app) == AS_APP_STATE_INSTALLED) {
		/* we have to do the little dance to appease the
		 * angry gnome controlling the state-machine */
		gs_app_set_state_internal (app, AS_APP_STATE_UNKNOWN);
		gs_app_set_state_internal (app, AS_APP_STATE_UPDATABLE);
	}

	/* save any properties we already know */
	if (priv2->sources->len > 0)
		gs_app_set_sources (app, priv2->sources);
//	if (priv2->project_group != NULL)
//		as_app_set_project_group (app, priv2->project_group, -1);
//	if (priv2->name != NULL)
//		gs_app_set_name (app, priv2->name_quality, priv2->name);
//	if (priv2->summary != NULL)
//		gs_app_set_summary (app, priv2->summary_quality, priv2->summary);
//	if (priv2->description != NULL)
//		gs_app_set_description (app, priv2->description_quality, priv2->description);
	if (priv2->update_details != NULL)
		gs_app_set_update_details (app, priv2->update_details);
	if (priv2->update_version != NULL)
		gs_app_set_update_version_internal (app, priv2->update_version);
	if (priv2->pixbuf != NULL)
		gs_app_set_pixbuf (app, priv2->pixbuf);
//	if (priv->categories != priv2->categories) {
//		for (i = 0; i < priv2->categories->len; i++) {
//			tmp = g_ptr_array_index (priv2->categories, i);
//			as_app_has_category (app, tmp);
//		}
//	}
	for (i = 0; i < priv2->related->len; i++) {
		app_tmp = g_ptr_array_index (priv2->related, i);
		gs_app_add_related (app, app_tmp);
	}
	priv->kudos |= priv2->kudos;

	/* copy metadata from @other to @app unless the app already has a key
	 * of that name */
//	keys = g_hash_table_get_keys (priv2->metadata);
//	for (l = keys; l != NULL; l = l->next) {
//		tmp = g_hash_table_lookup (as_app_get_metadata (app), l->data);
//		if (tmp != NULL)
//			continue;
//		tmp = g_hash_table_lookup (priv2->metadata, l->data);
//		as_app_add_metadata (app, l->data, tmp, -1);
//	}
//	g_list_free (keys);

	as_app_subsume (app, other);
}

/**
 * gs_app_get_property:
 */
static void
gs_app_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	AsApp *app = AS_APP (object);
	GsAppPrivate *priv = GS_APP(app)->priv;

	switch (prop_id) {
	case PROP_ID:
		g_value_set_string (value, as_app_get_id (app));
		break;
	case PROP_NAME:
		g_value_set_string (value, as_app_get_name (app, NULL));
		break;
	case PROP_VERSION:
		g_value_set_string (value, priv->version);
		break;
	case PROP_SUMMARY:
		g_value_set_string (value, as_app_get_comment (app, NULL));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, as_app_get_description (app, NULL));
		break;
	case PROP_RATING:
		g_value_set_uint (value, priv->rating);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_STATE:
		g_value_set_uint (value, as_app_get_state (app));
		break;
	case PROP_INSTALL_DATE:
		g_value_set_uint64 (value, priv->install_date);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gs_app_set_property:
 */
static void
gs_app_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	AsApp *app = AS_APP (object);

	switch (prop_id) {
	case PROP_ID:
		as_app_set_id_full (app, g_value_get_string (value), -1);
		break;
	case PROP_NAME:
		gs_app_set_name (app,
				 GS_APP_QUALITY_UNKNOWN,
				 g_value_get_string (value));
		break;
	case PROP_VERSION:
		gs_app_set_version (app, g_value_get_string (value));
		break;
	case PROP_SUMMARY:
		gs_app_set_summary (app,
				    GS_APP_QUALITY_UNKNOWN,
				    g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		gs_app_set_description (app,
					GS_APP_QUALITY_UNKNOWN,
					g_value_get_string (value));
		break;
	case PROP_RATING:
		gs_app_set_rating (app, g_value_get_int (value));
		break;
	case PROP_KIND:
		gs_app_set_kind (app, g_value_get_uint (value));
		break;
	case PROP_STATE:
		gs_app_set_state_internal (app, g_value_get_uint (value));
		break;
	case PROP_INSTALL_DATE:
		gs_app_set_install_date (app, g_value_get_uint64 (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gs_app_class_init:
 * @klass: The GsAppClass
 **/
static void
gs_app_class_init (GsAppClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gs_app_finalize;
	object_class->get_property = gs_app_get_property;
	object_class->set_property = gs_app_set_property;

	/**
	 * AsApp:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * AsApp:name:
	 */
	pspec = g_param_spec_string ("name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_NAME, pspec);

	/**
	 * AsApp:version:
	 */
	pspec = g_param_spec_string ("version", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_VERSION, pspec);

	/**
	 * AsApp:summary:
	 */
	pspec = g_param_spec_string ("summary", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_SUMMARY, pspec);

	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * AsApp:rating:
	 */
	pspec = g_param_spec_int ("rating", NULL, NULL,
				  -1, 100, -1,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_RATING, pspec);

	/**
	 * AsApp:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   GS_APP_KIND_UNKNOWN,
				   GS_APP_KIND_LAST,
				   GS_APP_KIND_UNKNOWN,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * AsApp:state:
	 */
	pspec = g_param_spec_uint ("state", NULL, NULL,
				   AS_APP_STATE_UNKNOWN,
				   AS_APP_STATE_LAST,
				   AS_APP_STATE_UNKNOWN,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_STATE, pspec);

	pspec = g_param_spec_uint64 ("install-date", NULL, NULL,
				     0, G_MAXUINT64, 0,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_INSTALL_DATE, pspec);
}

/**
 * gs_app_init:
 **/
static void
gs_app_init (GsApp *app)
{
	app->priv = gs_app_get_instance_private (app);
	app->priv->rating = -1;
	app->priv->rating_confidence = -1;
	app->priv->rating_kind = GS_APP_RATING_KIND_UNKNOWN;
	app->priv->sources = g_ptr_array_new_with_free_func (g_free);
	app->priv->source_ids = g_ptr_array_new_with_free_func (g_free);
	app->priv->addons = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	app->priv->related = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	app->priv->history = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	app->priv->addons_hash = g_hash_table_new_full (g_str_hash,
	                                                g_str_equal,
	                                                g_free,
	                                                NULL);
	app->priv->related_hash = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 NULL);
}

/**
 * gs_app_finalize:
 * @object: The object to finalize
 **/
static void
gs_app_finalize (GObject *object)
{
	GsApp *app = GS_APP (object);
	GsAppPrivate *priv = app->priv;

	g_free (priv->menu_path);
	g_free (priv->origin);
	g_ptr_array_unref (priv->sources);
	g_ptr_array_unref (priv->source_ids);
	g_free (priv->version);
	g_free (priv->version_ui);
	g_free (priv->summary_missing);
	g_free (priv->update_version);
	g_free (priv->update_version_ui);
	g_free (priv->update_details);
	g_free (priv->management_plugin);
	g_hash_table_unref (priv->addons_hash);
	g_ptr_array_unref (priv->addons);
	g_hash_table_unref (priv->related_hash);
	g_ptr_array_unref (priv->related);
	g_ptr_array_unref (priv->history);
	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
	if (priv->featured_pixbuf != NULL)
		g_object_unref (priv->featured_pixbuf);

	G_OBJECT_CLASS (gs_app_parent_class)->finalize (object);
}

/**
 * gs_app_new:
 *
 * Return value: a new AsApp object.
 **/
AsApp *
gs_app_new (const gchar *id)
{
	AsApp *app;
	app = g_object_new (GS_TYPE_APP,
			    "id", id,
			    NULL);
	return AS_APP (app);
}

/* vim: set noexpandtab: */
