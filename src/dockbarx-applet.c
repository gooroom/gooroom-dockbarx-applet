/*
 * Copyright (C) 2015-2019 Gooroom <gooroom@gooroom.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pwd.h>

#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gio/gdesktopappinfo.h>

#include <json-c/json.h>
#include <panel-applet.h>

#include "panel-glib.h"
#include "dockbarx-applet.h"

#define GRM_USER	".grm-user"


struct _DockbarxAppletPrivate
{
	GtkWidget *socket;

	GSettings *dockbarx_settings;
	GSettings *blacklist_settings;

	guint timeout_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (DockbarxApplet, dockbarx_applet, PANEL_TYPE_APPLET)


json_object *
JSON_OBJECT_GET (json_object *root_obj, const char *key)
{
	if (!root_obj) return NULL;

	json_object *ret_obj = NULL;

	json_object_object_get_ex (root_obj, key, &ret_obj);

	return ret_obj;
}


static gchar *
get_grm_user_data (void)
{
	gchar *data = NULL, *file = NULL;

	file = g_strdup_printf ("/var/run/user/%d/gooroom/%s", getuid (), GRM_USER);

	if (!g_file_test (file, G_FILE_TEST_EXISTS))
		goto error;

	g_file_get_contents (file, &data, NULL, NULL);

error:
	g_free (file);

	return data;
}

static gboolean
download_with_wget (const gchar *download_url, const gchar *download_path)
{
	gboolean ret = FALSE;

	if (!download_url || !download_path)
		return FALSE;

	gchar *cmd = g_find_program_in_path ("wget");
	if (cmd) {
		gchar *cmdline = g_strdup_printf ("%s --no-check-certificate %s -q -O %s", cmd, download_url, download_path);
		ret = g_spawn_command_line_sync (cmdline, NULL, NULL, NULL, NULL);
		g_free (cmdline);
	}
	g_free (cmd);

	return ret;
}

static gchar *
download_favicon (const gchar *favicon_url, gint num)
{
	g_return_val_if_fail (favicon_url != NULL, NULL);

	gchar *favicon_path = NULL;

	favicon_path = g_strdup_printf ("%s/favicon-%.02d", g_get_user_cache_dir (), num);
	g_remove (favicon_path);

	if (!download_with_wget (favicon_url, favicon_path))
		goto error;

	if (!g_file_test (favicon_path, G_FILE_TEST_EXISTS))
		goto error;

	// check file size
	struct stat st;
	if (lstat (favicon_path, &st) == -1)
		goto error;

	if (st.st_size == 0)
		goto error;

	return favicon_path;

error:
	g_free (favicon_path);

	return g_strdup ("applications-other");
}


static gchar *
get_desktop_directory (json_object *obj)
{
	g_return_val_if_fail (obj != NULL, NULL);

	gchar *desktop_dir = NULL;
	const char *val = json_object_get_string (obj);

	if (g_strcmp0 (val, "bar") == 0) {
		desktop_dir = g_build_filename (g_get_user_data_dir (), "applications/custom", NULL);
	} else {
		desktop_dir = g_build_filename (g_get_user_data_dir () ,"applications", NULL);
	}

	if (!g_file_test (desktop_dir, G_FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (desktop_dir, 0755) == -1) {
			g_free (desktop_dir);
			return NULL;
		}
	}

	return desktop_dir;
}

static void
remove_desktop_files (void)
{
	gchar *remove_dir = g_build_filename (g_get_user_data_dir (), "applications/custom", NULL);
	if (g_file_test (remove_dir, G_FILE_TEST_EXISTS)) {
		gchar *cmd, *cmdline;

		cmd = g_find_program_in_path ("rm");
		cmdline = g_strdup_printf ("%s -rf %s", cmd, remove_dir);

		g_spawn_command_line_sync (cmdline, NULL, NULL, NULL, NULL);

		g_free (cmd);
		g_free (cmdline);
	}

	g_free (remove_dir);
}


static gboolean
create_desktop_file (json_object *obj, const gchar *dt_file_name, gint num)
{
	g_return_val_if_fail ((obj != NULL) || (dt_file_name != NULL), FALSE);

	gboolean ret = FALSE;
	GKeyFile *keyfile = NULL;

	keyfile = g_key_file_new ();

	json_object_object_foreach (obj, key, val) {
		const gchar *value = json_object_get_string (val);
		gchar *d_key = g_ascii_strdown (key, -1);

		if (d_key && g_strcmp0 (d_key, "icon") == 0) {
			if (g_str_has_prefix (value, "http://") || g_str_has_prefix (value, "https://")) {
				gchar *icon_file = download_favicon (value, num);
				if (icon_file) {
					g_key_file_set_string (keyfile, "Desktop Entry", "Icon", icon_file);
					g_free (icon_file);
				} else {
					g_key_file_set_string (keyfile, "Desktop Entry", "Icon", "applications-other");
				}
			} else {
				g_key_file_set_string (keyfile, "Desktop Entry", "Icon", value);
			}
		} else {
			gchar *new_key = NULL;

			if (g_strcmp0 (d_key, "name") == 0) {
				new_key = g_strdup ("Name");
			} else if (g_strcmp0 (d_key, "comment") == 0) {
				new_key = g_strdup ("Comment");
			} else if (g_strcmp0 (d_key, "exec") == 0) {
				new_key = g_strdup ("Exec");
			}

			if (new_key) {
				g_key_file_set_string (keyfile, "Desktop Entry", new_key, value);
				g_free (new_key);
			}
		}

		g_free (d_key);
	}

	g_key_file_set_string (keyfile, "Desktop Entry", "Type", "Application");
	g_key_file_set_string (keyfile, "Desktop Entry", "Terminal", "false");
	g_key_file_set_string (keyfile, "Desktop Entry", "StartupNotify", "true");
	/* we don't want to show in application launcher */
	g_key_file_set_string (keyfile, "Desktop Entry", "NoDisplay", "true");

	ret = g_key_file_save_to_file (keyfile, dt_file_name, NULL);

	g_key_file_free (keyfile);

	return ret;
}

static void
dockbarx_launchers_set (GSList *launchers, DockbarxApplet *applet)
{
	DockbarxAppletPrivate *priv = applet->priv;

	if (launchers && g_slist_length (launchers) > 0) {
		gchar **strings;
		GPtrArray *array = g_ptr_array_new ();

		GSList *l = NULL;
		for (l = launchers; l; l = l->next) {
			g_ptr_array_add (array, g_strdup ((gchar *)l->data));
		}
		g_ptr_array_add (array, NULL);

		strings = (gchar **)g_ptr_array_free (array, FALSE);
		g_settings_set_strv (priv->dockbarx_settings, "launchers", (const char * const *)strings);
		g_strfreev (strings);
	} else {
		g_settings_set_strv (priv->dockbarx_settings, "launchers", NULL);
	}
}

static gboolean
contain_launcher (GSList *launchers, const gchar *launcher)
{
	gboolean ret = FALSE;
	gchar *cmp_name = NULL;
	gchar *cmp_exec = NULL;
	GDesktopAppInfo *cmp_dt_info = NULL;

	cmp_dt_info = g_desktop_app_info_new (launcher);

	if (cmp_dt_info) return ret;

	cmp_name = g_desktop_app_info_get_string (cmp_dt_info, G_KEY_FILE_DESKTOP_KEY_NAME);
	cmp_exec = g_desktop_app_info_get_string (cmp_dt_info, G_KEY_FILE_DESKTOP_KEY_EXEC);

	GSList *l = NULL;
	for (l = launchers; l; l = l->next) {
		gchar *id = (gchar *)l->data;
		GDesktopAppInfo *dt_info = g_desktop_app_info_new (id);
		if (dt_info) {
			gchar *name = g_desktop_app_info_get_string (dt_info, G_KEY_FILE_DESKTOP_KEY_NAME);
			if (name && cmp_name && g_str_equal (name, cmp_name)) {
				g_object_unref (dt_info);
				g_free (name);
				ret = TRUE;
				break;
			}

			gchar *exec = g_desktop_app_info_get_string (dt_info, G_KEY_FILE_DESKTOP_KEY_EXEC);
			if (exec && cmp_exec && g_str_equal (exec, cmp_exec)) {
				g_object_unref (dt_info);
				g_free (exec);
				ret = TRUE;
				break;
			}

			g_object_unref (dt_info);
		}
	}

	return ret;
}

static GSList *
combine_launchers (GSList *old_launchers, GSList *new_launchers)
{
	GSList *ret_launchers = NULL;

	if (!old_launchers && !new_launchers)
		return NULL;

	if (old_launchers && !new_launchers)
		return g_slist_copy_deep (old_launchers, (GCopyFunc)g_strdup, NULL);

	if (!old_launchers && new_launchers)
		return g_slist_copy_deep (new_launchers, (GCopyFunc)g_strdup, NULL);

	ret_launchers = g_slist_copy_deep (old_launchers, (GCopyFunc)g_strdup, NULL);

	GSList *l = NULL;
	for (l = new_launchers; l; l = l->next) {
		gchar *new_launcher = (gchar *)l->data;
		if (!contain_launcher (ret_launchers, new_launcher)) {
			ret_launchers = g_slist_append (ret_launchers, g_strdup (new_launcher));
		}
	}

	return ret_launchers;
}

static GSList *
dockbarx_launchers_get (DockbarxApplet *applet)
{
	GSList *ret = NULL;
	DockbarxAppletPrivate *priv = applet->priv;

	if (priv->dockbarx_settings) {
		guint i;
		gchar **launchers;

		launchers = g_settings_get_strv (priv->dockbarx_settings, "launchers");
		for (i = 0; i < g_strv_length (launchers); i++) {
			if (!g_str_has_prefix (launchers[i], "shortcut-"))
				ret = g_slist_append (ret, g_strdup (launchers[i]));
		}

		g_strfreev (launchers);
	}

	return ret;
}

static GSList *
get_launchers_from_online (json_object *root_obj)
{
	if (!root_obj)
		return NULL;

	GSList *launchers = NULL;
	json_object *apps_obj = NULL;

	apps_obj = JSON_OBJECT_GET (root_obj, "apps");
	if (apps_obj) {
		gint i = 0, len = 0;;
		len = json_object_array_length (apps_obj);
		for (i = 0; i < len; i++) {
			json_object *app_obj = json_object_array_get_idx (apps_obj, i);

			if (app_obj) {
				json_object *dt_obj = NULL, *pos_obj = NULL;
				dt_obj = JSON_OBJECT_GET (app_obj, "desktop");
				pos_obj = JSON_OBJECT_GET (app_obj, "position");

				if (dt_obj && pos_obj) {
					gchar *dt_dir_name = get_desktop_directory (pos_obj);
					if (dt_dir_name) {
						gchar *dt_file_name = g_strdup_printf ("%s/shortcut-%.02d.desktop", dt_dir_name, i);

						if (create_desktop_file (dt_obj, dt_file_name, i)) {
							gchar *launcher = g_strdup_printf ("shortcut-%.02d;%s", i, dt_file_name);
							launchers = g_slist_append (launchers, launcher);
						} else {
							g_error ("Could not create desktop file : %s", dt_file_name);
						}

						g_free (dt_file_name);
						g_free (dt_dir_name);
					}
				}
			}
		}
	}

	return launchers;
}

static GSList *
get_launchers (json_object *root_obj, DockbarxApplet *applet)
{
	GSList *cmb_launchers = NULL;
	GSList *old_launchers = NULL;
	GSList *new_launchers = NULL;

	old_launchers = dockbarx_launchers_get (applet);
	new_launchers = get_launchers_from_online (root_obj);

	cmb_launchers = combine_launchers (old_launchers, new_launchers);

	g_slist_free_full (old_launchers, (GDestroyNotify) g_free);
	g_slist_free_full (new_launchers, (GDestroyNotify) g_free);

	return cmb_launchers;
}

static void
dockbarx_launcher_update (DockbarxApplet *applet)
{
	GSList *launchers = NULL;
	gchar *data = get_grm_user_data ();

	if (data) {
		enum json_tokener_error jerr = json_tokener_success;
		json_object *root_obj = json_tokener_parse_verbose (data, &jerr);
		if (jerr == json_tokener_success) {
			json_object *obj1 = NULL, *obj2= NULL;
			obj1 = JSON_OBJECT_GET (root_obj, "data");
			obj2 = JSON_OBJECT_GET (obj1, "desktopInfo");
			launchers = get_launchers (obj2, applet);
			json_object_put (root_obj);
		}
		g_free (data);
	} else {
		launchers = get_launchers (NULL, applet);
	}

	dockbarx_launchers_set (launchers, applet);
}

static void
dockbarx_launchers_config_set (DockbarxApplet *applet)
{
	DockbarxAppletPrivate *priv = applet->priv;

	remove_desktop_files ();

	dockbarx_launcher_update (applet);
}

static void
start_dockbarx (DockbarxApplet *applet)
{
	DockbarxAppletPrivate *priv = applet->priv;

	gchar *cmd = NULL;
	gulong socket_id = 0;

	gtk_widget_destroy (priv->socket);
	priv->socket = gtk_socket_new ();
	gtk_container_add (GTK_CONTAINER (applet), priv->socket);
	gtk_widget_show (GTK_WIDGET (priv->socket));

	socket_id = gtk_socket_get_id (GTK_SOCKET (priv->socket));

	cmd = g_strdup_printf ("/usr/bin/env python2 %s -s %lu", DOCKBARX_PLUG, socket_id);

	g_spawn_command_line_async (cmd, NULL);
}

static gboolean
start_dockbarx_idle (gpointer data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	start_dockbarx (applet);

	gtk_widget_show_all (GTK_WIDGET (applet));

	return FALSE;
}

static void
process_done_cb (GPid pid, gint status, gpointer data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	g_spawn_close_pid (pid);

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	priv->timeout_id = g_timeout_add (500, (GSourceFunc)start_dockbarx_idle, applet);
}

static void
update_dockbarx (DockbarxApplet *applet)
{
	GPid pid;
	gchar **argv = NULL, **envp = NULL;
	DockbarxAppletPrivate *priv = applet->priv;

	dockbarx_launchers_config_set (applet);

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	envp = g_get_environ ();
	g_shell_parse_argv ("/usr/bin/pkill -f 'python.*xfce4-dockbarx-plug'", NULL, &argv, NULL);

	if (g_spawn_async (NULL, argv, envp, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL)) {
		g_child_watch_add (pid, (GChildWatchFunc) process_done_cb, applet);
	} else {
		priv->timeout_id = g_timeout_add (500, (GSourceFunc)start_dockbarx_idle, applet);
	}

	g_strfreev (argv);
}

static gboolean
update_dockbarx_idle (gpointer data)
{
	update_dockbarx (DOCKBARX_APPLET (data));

	return FALSE;
}

static void
blacklist_settings_changed_cb (GSettings   *settings,
                               const gchar *key,
                               gpointer     user_data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (user_data);

	if (key && g_str_equal (key, "blacklist"))
		g_idle_add ((GSourceFunc)update_dockbarx_idle, applet);
}

static gboolean
set_max_size_cb (gpointer data)
{
	gint size;
	GtkAllocation alloc;
	GtkOrientation orientation;

	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	orientation = panel_applet_get_gtk_orientation (PANEL_APPLET (applet));
	gtk_widget_get_allocation (GTK_WIDGET (applet), &alloc);

	size = (orientation == GTK_ORIENTATION_HORIZONTAL) ? alloc.width : alloc.height;

	g_settings_set_int (priv->dockbarx_settings, "max-size", size);

	return FALSE;
}

static void
dockbarx_applet_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
	PanelApplet *panel_applet = PANEL_APPLET (widget);

	GTK_WIDGET_CLASS (dockbarx_applet_parent_class)->size_allocate (widget, allocation);

	gint size_hints[2];
	size_hints[0] = 32767;
	size_hints[1] = 0;

	panel_applet_set_size_hints (panel_applet, size_hints, 2, 0);

	g_timeout_add (100, (GSourceFunc)set_max_size_cb, panel_applet);
}

static void
dockbarx_applet_finalize (GObject *object)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (object);
	DockbarxAppletPrivate *priv = applet->priv;

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->dockbarx_settings)
		g_object_unref (priv->dockbarx_settings);

	if (priv->blacklist_settings)
		g_object_unref (priv->blacklist_settings);

	if (G_OBJECT_CLASS (dockbarx_applet_parent_class)->finalize)
		G_OBJECT_CLASS (dockbarx_applet_parent_class)->finalize (object);
}

static void
dockbarx_applet_init (DockbarxApplet *applet)
{
	GSettingsSchema *schema = NULL;

	DockbarxAppletPrivate *priv;

	priv = applet->priv = dockbarx_applet_get_instance_private (applet);

	panel_applet_set_flags (PANEL_APPLET (applet),
                            PANEL_APPLET_EXPAND_MAJOR |
                            PANEL_APPLET_EXPAND_MINOR);


	schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (), "org.dockbarx", TRUE);
	if (schema) {
		priv->dockbarx_settings = g_settings_new_full (schema, NULL, NULL);
		g_settings_schema_unref (schema);
	}

	schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (), "apps.gooroom-applauncher-applet", TRUE);
	if (schema) {
		priv->blacklist_settings = g_settings_new_full (schema, NULL, NULL);

		g_signal_connect (priv->blacklist_settings, "changed",
                          G_CALLBACK (blacklist_settings_changed_cb), applet);

		g_settings_schema_unref (schema);
	}

	priv->timeout_id = 0;

	priv->socket = gtk_socket_new ();
	gtk_container_add (GTK_CONTAINER (applet), priv->socket);
	gtk_widget_show (GTK_WIDGET (priv->socket));
}

static void
dockbarx_applet_class_init (DockbarxAppletClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	object_class->finalize = dockbarx_applet_finalize;
	widget_class->size_allocate = dockbarx_applet_size_allocate;
}

static gboolean
dockbarx_applet_fill (DockbarxApplet *applet)
{
	update_dockbarx (applet);

	return TRUE;
}

static gboolean
dockbarx_applet_factory (PanelApplet *applet,
                         const gchar *iid,
                         gpointer     data)
{
	gboolean retval = FALSE;

	if (!g_strcmp0 (iid, "DockbarxApplet"))
		retval = dockbarx_applet_fill (DOCKBARX_APPLET (applet));

	return retval;
}

PANEL_APPLET_IN_PROCESS_FACTORY ("DockbarxAppletFactory",
                   DOCKBARX_TYPE_APPLET,
                   (PanelAppletFactoryCallback)dockbarx_applet_factory,
                   NULL)
