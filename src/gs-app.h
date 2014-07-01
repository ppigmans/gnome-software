/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
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

#ifndef __GS_APP_H
#define __GS_APP_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <appstream-glib.h>

G_BEGIN_DECLS

#define GS_TYPE_APP		(gs_app_get_type ())
#define GS_APP(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_APP, GsApp))
#define GS_APP_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GS_TYPE_APP, GsAppClass))
#define GS_IS_APP(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_APP))
#define GS_IS_APP_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_APP))

typedef struct GsAppPrivate GsAppPrivate;

typedef struct
{
	 AsApp			 parent;
	 GsAppPrivate		*priv;
} GsApp;

typedef struct
{
	AsAppClass		 parent_class;
} GsAppClass;

typedef enum {
	GS_APP_ERROR_FAILED,
	GS_APP_ERROR_LAST
} GsAppError;

typedef enum {
	GS_APP_KIND_UNKNOWN,
	GS_APP_KIND_NORMAL,	/* app	[ install:1 remove:1 update:1 ] */
	GS_APP_KIND_SYSTEM,	/* app	[ install:0 remove:0 update:1 ] */
	GS_APP_KIND_PACKAGE,	/* pkg	[ install:0 remove:0 update:1 ] */
	GS_APP_KIND_OS_UPDATE,	/* pkg	[ install:0 remove:0 update:1 ] */
	GS_APP_KIND_MISSING,	/* meta	[ install:0 remove:0 update:0 ] */
	GS_APP_KIND_SOURCE,	/* src	[ install:1 remove:0 update:0 ] */
	GS_APP_KIND_CORE,	/* pkg	[ install:0 remove:0 update:1 ] */
	GS_APP_KIND_LAST
} GsAppKind;

typedef enum {
	GS_APP_RATING_KIND_UNKNOWN,
	GS_APP_RATING_KIND_USER,
	GS_APP_RATING_KIND_SYSTEM,
	GS_APP_RATING_KIND_KUDOS,
	GS_APP_RATING_KIND_LAST
} GsAppRatingKind;

typedef enum {
	GS_APP_KUDO_MY_LANGUAGE			= 1 << 0,
	GS_APP_KUDO_RECENT_RELEASE		= 1 << 1,
	GS_APP_KUDO_FEATURED_RECOMMENDED	= 1 << 2,
	GS_APP_KUDO_MODERN_TOOLKIT		= 1 << 3,
	GS_APP_KUDO_SEARCH_PROVIDER		= 1 << 4,
	GS_APP_KUDO_INSTALLS_USER_DOCS		= 1 << 5,
	GS_APP_KUDO_USES_NOTIFICATIONS		= 1 << 6,
	GS_APP_KUDO_HAS_KEYWORDS		= 1 << 7,
	GS_APP_KUDO_USES_APP_MENU		= 1 << 8,
	GS_APP_KUDO_HAS_SCREENSHOTS		= 1 << 9,
	GS_APP_KUDO_POPULAR			= 1 << 10,
	GS_APP_KUDO_IBUS_HAS_SYMBOL		= 1 << 11,
	GS_APP_KUDO_LAST
} GsAppKudo;

#define	GS_APP_INSTALL_DATE_UNSET		0
#define	GS_APP_INSTALL_DATE_UNKNOWN		1 /* 1s past the epoch */
#define	GS_APP_SIZE_UNKNOWN			0
#define	GS_APP_SIZE_MISSING			1

typedef enum {
	GS_APP_QUALITY_UNKNOWN,
	GS_APP_QUALITY_LOWEST,
	GS_APP_QUALITY_NORMAL,
	GS_APP_QUALITY_HIGHEST,
	GS_APP_QUALITY_LAST
} GsAppQuality;

#define	GS_APP_KUDOS_WEIGHT_TO_PERCENTAGE(w)	(w * 20)

GType		 gs_app_get_type		(void);

AsApp		*gs_app_new			(const gchar	*id);
gchar		*gs_app_to_string		(AsApp		*app);
const gchar	*gs_app_kind_to_string		(GsAppKind	 kind);

void		 gs_app_subsume			(AsApp		*app,
						 AsApp		*other);

GsAppKind	 gs_app_get_kind		(AsApp		*app);
void		 gs_app_set_kind		(AsApp		*app,
						 GsAppKind	 kind);
void		 gs_app_set_state		(AsApp		*app,
						 AsAppState	 state);
void		 gs_app_set_name		(AsApp		*app,
						 GsAppQuality	 quality,
						 const gchar	*name);
const gchar	*gs_app_get_source_default	(AsApp		*app);
void		 gs_app_add_source		(AsApp		*app,
						 const gchar	*source);
GPtrArray	*gs_app_get_sources		(AsApp		*app);
void		 gs_app_set_sources		(AsApp		*app,
						 GPtrArray	*sources);
const gchar	*gs_app_get_source_id_default	(AsApp		*app);
void		 gs_app_add_source_id		(AsApp		*app,
						 const gchar	*source_id);
GPtrArray	*gs_app_get_source_ids		(AsApp		*app);
void		 gs_app_set_source_ids		(AsApp		*app,
						 GPtrArray	*source_ids);
const gchar	*gs_app_get_version		(AsApp		*app);
const gchar	*gs_app_get_version_ui		(AsApp		*app);
void		 gs_app_set_version		(AsApp		*app,
						 const gchar	*version);
void		 gs_app_set_summary		(AsApp		*app,
						 GsAppQuality	 quality,
						 const gchar	*summary);
const gchar	*gs_app_get_summary_missing	(AsApp		*app);
void		 gs_app_set_summary_missing	(AsApp		*app,
						 const gchar	*missing);
void		 gs_app_set_description		(AsApp		*app,
						 GsAppQuality	 quality,
						 const gchar	*description);
const gchar	*gs_app_get_menu_path		(AsApp		*app);
void		 gs_app_set_menu_path		(AsApp		*app,
						 const gchar	*menu_path);
const gchar	*gs_app_get_origin		(AsApp		*app);
void		 gs_app_set_origin		(AsApp		*app,
						 const gchar	*origin);
const gchar	*gs_app_get_update_version	(AsApp		*app);
const gchar	*gs_app_get_update_version_ui	(AsApp		*app);
void		 gs_app_set_update_version	(AsApp		*app,
						 const gchar	*update_version);
const gchar	*gs_app_get_update_details	(AsApp		*app);
void		 gs_app_set_update_details	(AsApp		*app,
						 const gchar	*update_details);
const gchar	*gs_app_get_management_plugin	(AsApp		*app);
void		 gs_app_set_management_plugin	(AsApp		*app,
						 const gchar	*management_plugin);
GdkPixbuf	*gs_app_get_pixbuf		(AsApp		*app);
void		 gs_app_set_pixbuf		(AsApp		*app,
						 GdkPixbuf	*pixbuf);
gboolean	 gs_app_load_icon		(AsApp		*app,
						 GError		**error);
GdkPixbuf	*gs_app_get_featured_pixbuf	(AsApp		*app);
void		 gs_app_set_featured_pixbuf	(AsApp		*app,
						 GdkPixbuf	*pixbuf);
gint		 gs_app_get_rating		(AsApp		*app);
void		 gs_app_set_rating		(AsApp		*app,
						 gint		 rating);
gint		 gs_app_get_rating_confidence	(AsApp		*app);
void		 gs_app_set_rating_confidence	(AsApp		*app,
						 gint		 rating_confidence);
GsAppRatingKind	 gs_app_get_rating_kind		(AsApp		*app);
void		 gs_app_set_rating_kind		(AsApp		*app,
						 GsAppRatingKind rating_kind);
guint64		 gs_app_get_size		(AsApp		*app);
void		 gs_app_set_size		(AsApp		*app,
						 guint64	 size);
GPtrArray	*gs_app_get_addons		(AsApp		*app);
void		 gs_app_add_addon		(AsApp		*app,
						 AsApp		*addon);
GPtrArray	*gs_app_get_related		(AsApp		*app);
void		 gs_app_add_related		(AsApp		*app,
						 AsApp		*app2);
GPtrArray	*gs_app_get_history		(AsApp		*app);
void		 gs_app_add_history		(AsApp		*app,
						 AsApp		*app2);
guint64		 gs_app_get_install_date	(AsApp		*app);
void		 gs_app_set_install_date	(AsApp		*app,
						 guint64	 install_date);
void		 gs_app_add_kudo		(AsApp		*app,
						 GsAppKudo	 kudo);
guint64		 gs_app_get_kudos		(AsApp		*app);
guint		 gs_app_get_kudos_weight	(AsApp		*app);
guint		 gs_app_get_kudos_percentage	(AsApp		*app);
gboolean	 gs_app_get_to_be_installed	(AsApp		*app);
void		 gs_app_set_to_be_installed	(AsApp		*app,
						 gboolean	 to_be_installed);

G_END_DECLS

#endif /* __GS_APP_H */

/* vim: set noexpandtab: */
