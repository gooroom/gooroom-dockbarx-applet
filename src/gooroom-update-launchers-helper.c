/*
 * Copyright (C) 2018-2021 Gooroom <gooroom@gooroom.kr>
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


//#include <pwd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <json-c/json.h>

#include "panel-glib.h"

#define GRM_USER	".grm-user"
#define MAX_RETRY	10

static gint retry = 0;

static json_object *
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

	file = g_strdup_printf ("%s/.gooroom/%s", g_get_home_dir (), GRM_USER);

	if (!g_file_test (file, G_FILE_TEST_EXISTS))
		goto error;

	g_file_get_contents (file, &data, NULL, NULL);

error:
	g_free (file);

	return data;
}

static void
download_with_wget (const gchar *download_url, const gchar *download_path)
{
	if (!download_url || !download_path)
		return;

	gchar *wget = g_find_program_in_path ("wget");
	if (wget) {
		gchar *cmd = g_strdup_printf ("%s --no-check-certificate %s -q -O %s", wget, download_url, download_path);
		g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
		g_free (cmd);
	}
	g_free (wget);
}

static gchar *
download_favicon (const gchar *favicon_url, gint num)
{
	g_return_val_if_fail (favicon_url != NULL, NULL);

	gint retry = 1;
	gchar *favicon_path = NULL, *ret = NULL;

	favicon_path = g_strdup_printf ("%s/favicon-%.02d", g_get_user_cache_dir (), num);

	download_with_wget (favicon_url, favicon_path);

	while (1) {
		g_usleep (10000);

		if (retry++ > MAX_RETRY ||
            g_file_test (favicon_path, G_FILE_TEST_EXISTS)) {
			break;
		}
		download_with_wget (favicon_url, favicon_path);
	}

	if (!g_file_test (favicon_path, G_FILE_TEST_EXISTS)) {
		ret = g_strdup ("applications-other");
		goto error;
	}

	// check file type
	gchar *file = NULL, *mime_type = NULL;

	file = g_find_program_in_path ("file");
	if (file) {
		gchar *cmd, *output = NULL;

		cmd = g_strdup_printf ("%s --brief --mime-type %s", file, favicon_path);
		if (g_spawn_command_line_sync (cmd, &output, NULL, NULL, NULL)) {
			gchar **lines = g_strsplit (output, "\n", -1);
			if (g_strv_length (lines) > 0)
				mime_type = g_strdup (lines[0]);
			g_strfreev (lines);
		}

		g_free (cmd);
		g_free (output);
	}

	g_free (file);

	if (!g_str_equal (mime_type, "image/png") &&
        !g_str_equal (mime_type, "image/jpg") &&
        !g_str_equal (mime_type, "image/jpeg") &&
        !g_str_equal (mime_type, "image/svg")) {
		ret = g_strdup ("applications-other");
	} else {
		ret = g_strdup (favicon_path);
	}

	g_free (mime_type);

error:
	g_free (favicon_path);

	return ret;
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
cleanup_favicon_files (void)
{
	gchar *rm, *cmd;

	rm = g_find_program_in_path ("rm");
	cmd = g_strdup_printf ("%s -f %s/favicon*", rm, g_get_user_cache_dir ());

	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

	g_free (rm);
	g_free (cmd);
}

static void
cleanup_desktop_files (void)
{
	gchar *remove_dir = g_build_filename (g_get_user_data_dir (), "applications/custom", NULL);
	if (g_file_test (remove_dir, G_FILE_TEST_EXISTS)) {
		gchar *rm, *cmd;

		rm = g_find_program_in_path ("rm");
		cmd = g_strdup_printf ("%s -rf %s", rm, remove_dir);

		g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

		g_free (rm);
		g_free (cmd);
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

        if (d_key && g_strcmp0 (d_key, "exec") == 0) {
            if (g_strstr_len (value, -1, ",,,")) {
                gint i = 0;
                gchar **s_exec = g_strsplit (value, ",,,", -1);

                for (i = 0; s_exec[i] != NULL; i++) {
                    if (g_find_program_in_path (s_exec[i])) {
                        value = g_strdup (s_exec[i]);
                    }
                }
                g_strfreev (s_exec);
            }
        }

        if (d_key && g_strcmp0 (d_key, "icon") == 0) {
            if (g_str_has_prefix (value, "http://") || g_str_has_prefix (value, "https://")) {
                gchar *icon_file = download_favicon (value, num);
                g_key_file_set_string (keyfile, "Desktop Entry", "Icon", icon_file);
                g_free (icon_file);
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
launchers_set (GSList *launchers, GSettings *dockbarx_settings)
{
	if (!dockbarx_settings)
		return;

	if (launchers && g_slist_length (launchers) > 0) {
		gchar *cmd, *strlist;
		gchar **strings;
		GPtrArray *array;

		array = g_ptr_array_new ();

		GSList *l = NULL;
		for (l = launchers; l; l = l->next) {
			g_ptr_array_add (array, g_strdup_printf ("'%s'", (gchar *)l->data));
		}
		g_ptr_array_add (array, NULL);

		strings = (gchar **)g_ptr_array_free (array, FALSE);
		strlist = g_strjoinv (",", strings);

		cmd = g_strdup_printf ("/usr/bin/gsettings set org.dockbarx launchers \"[%s]\"", strlist);
		g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

		g_free (cmd);
		g_strfreev (strings);
		g_free (strlist);
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
dockbarx_launchers_get (GSettings *dockbarx_settings)
{
	GSList *ret = NULL;

	if (!dockbarx_settings)
		return NULL;

	guint i;
	gchar **launchers;

	launchers = g_settings_get_strv (dockbarx_settings, "launchers");
	for (i = 0; i < g_strv_length (launchers); i++) {
		if (!g_str_has_prefix (launchers[i], "shortcut-"))
			ret = g_slist_append (ret, g_strdup (launchers[i]));
	}

	g_strfreev (launchers);

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
get_launchers (json_object *root_obj, GSettings *dockbarx_settings)
{
	GSList *cmb_launchers = NULL;
	GSList *old_launchers = NULL;
	GSList *new_launchers = NULL;

	old_launchers = dockbarx_launchers_get (dockbarx_settings);
	new_launchers = get_launchers_from_online (root_obj);

	cmb_launchers = combine_launchers (old_launchers, new_launchers);

	g_slist_free_full (old_launchers, (GDestroyNotify) g_free);
	g_slist_free_full (new_launchers, (GDestroyNotify) g_free);

	return cmb_launchers;
}

static gboolean
start_idle (gpointer user_data)
{
	GMainLoop *loop = NULL;
	GSList *launchers = NULL;
	gchar *data = NULL, *file = NULL;
	GSettingsSchema *schema = NULL;
    GSettings *dockbarx_settings = NULL;

	loop = (GMainLoop *)user_data;

	file = g_strdup_printf ("%s/.gooroom/%s", g_get_home_dir (), GRM_USER);

	if (!g_file_test (file, G_FILE_TEST_EXISTS)) {
		g_free (file);
		if (retry++ > MAX_RETRY) {
			g_main_loop_quit (loop);
			return FALSE;
		}
		return TRUE;
	}

	retry = 0;

	schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                              "org.dockbarx", TRUE);
	if (schema) {
		dockbarx_settings = g_settings_new_full (schema, NULL, NULL);
		g_settings_schema_unref (schema);
	}

	cleanup_desktop_files ();
	cleanup_favicon_files ();

	data = get_grm_user_data ();
	if (data) {
		enum json_tokener_error jerr = json_tokener_success;
		json_object *root_obj = json_tokener_parse_verbose (data, &jerr);
		if (jerr == json_tokener_success) {
			json_object *obj1 = NULL, *obj2= NULL;
			obj1 = JSON_OBJECT_GET (root_obj, "data");
			obj2 = JSON_OBJECT_GET (obj1, "desktopInfo");
			launchers = get_launchers (obj2, dockbarx_settings);
			json_object_put (root_obj);
		}
		g_free (data);
	} else {
		launchers = get_launchers (NULL, dockbarx_settings);
	}

	launchers_set (launchers, dockbarx_settings);

	g_free (file);

	if (dockbarx_settings)
		g_object_unref (dockbarx_settings);

	g_main_loop_quit (loop);

	return FALSE;
}

int
main (int argc, char **argv)
{
	GMainLoop *loop;

	loop = g_main_loop_new (NULL, FALSE);

	g_unix_signal_add (SIGINT,  (GSourceFunc) g_main_loop_quit, loop);
	g_unix_signal_add (SIGTERM, (GSourceFunc) g_main_loop_quit, loop);
	signal (SIGTSTP, SIG_IGN);

	g_timeout_add (100, (GSourceFunc)start_idle, loop);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	return 0;
}
