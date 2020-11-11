/*
 * Copyright (C) 2018-2019 Gooroom <gooroom@gooroom.kr>
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
#include <libgnome-panel/gp-applet.h>

#include "panel-glib.h"
#include "dockbarx-applet.h"

#define GRM_USER	".grm-user"


struct _DockbarxAppletPrivate
{
	GtkWidget *socket;

	GSettings *dockbarx_settings;

	guint reg_id;
	guint owner_id;
	guint timeout_id;

	GDBusConnection *connection;
};

G_DEFINE_TYPE_WITH_PRIVATE (DockbarxApplet, dockbarx_applet, GP_TYPE_APPLET)


static void handle_method_call (GDBusConnection *conn,
                                const gchar *sender,
                                const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *method_name,
                                GVariant *parameters,
                                GDBusMethodInvocation *invocation,
                                gpointer data);




static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='kr.gooroom.dockbarx.applet'>"
    "    <method name='Restart'/>"
    "  </interface>"
    "</node>";

static const GDBusInterfaceVTable interface_vtable = {
    handle_method_call,
    NULL,
    NULL
};

static void
on_dockbarx_applet_name_lost (GDBusConnection *connection,
                              const gchar     *name,
                              gpointer        *data)
{
}


static void
on_dockbarx_applet_bus_acquired (GDBusConnection *connection,
                                 const gchar     *name,
                                 gpointer        *data)
{
	GDBusNodeInfo *introspection_data;

	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	priv->connection = connection;

	// register object
	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	priv->reg_id = g_dbus_connection_register_object (priv->connection,
                                                      "/kr/gooroom/dockbarx/applet",
                                                      introspection_data->interfaces[0],
                                                      &interface_vtable,
                                                      applet, NULL,
                                                      NULL);
}

static void
dockbarx_applet_dbus_init (DockbarxApplet *applet)
{
	DockbarxAppletPrivate *priv = applet->priv;

	if (priv->owner_id == 0) {
		priv->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                         "kr.gooroom.dockbarx.applet",
                                         G_BUS_NAME_OWNER_FLAGS_NONE,
                                         (GBusAcquiredCallback) on_dockbarx_applet_bus_acquired,
                                         NULL,
                                         (GBusNameLostCallback) on_dockbarx_applet_name_lost,
                                         applet, NULL);
	}
}

static gboolean
dockbarx_applet_dbus_init_idle (gpointer data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (data);

	dockbarx_applet_dbus_init (applet);

	return FALSE;
}

static void
start_dockbarx_done_cb (GPid pid, gint status, gpointer data)
{
	g_spawn_close_pid (pid);
}

static gboolean
start_dockbarx (DockbarxApplet *applet)
{
	GPid pid;
	gchar *cmd = NULL;
	gchar **argv = NULL, **envp = NULL;
	gulong socket_id = 0;
	DockbarxAppletPrivate *priv = applet->priv;

	if (priv->socket) {
		gtk_widget_destroy (priv->socket);
		priv->socket = NULL;
	}

	priv->socket = gtk_socket_new ();
	gtk_container_add (GTK_CONTAINER (applet), priv->socket);
	gtk_widget_show (GTK_WIDGET (priv->socket));

	socket_id = gtk_socket_get_id (GTK_SOCKET (priv->socket));

	cmd = g_strdup_printf ("/usr/bin/env python3 %s -s %lu", DOCKBARX_PLUG, socket_id);

	envp = g_get_environ ();
	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	if (g_spawn_async (NULL, argv, envp, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL)) {
		g_child_watch_add (pid, (GChildWatchFunc) start_dockbarx_done_cb, applet);
	}

	g_timeout_add (500, (GSourceFunc)dockbarx_applet_dbus_init_idle, applet);

	g_free (cmd);
	g_strfreev (argv);

	return FALSE;
}

static void
kill_dockbarx_done_cb (GPid pid, gint status, gpointer data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	g_spawn_close_pid (pid);

	g_idle_add ((GSourceFunc)start_dockbarx, applet);
}

static void
kill_dockbarx (DockbarxApplet *applet)
{
	GPid pid;
	gchar **argv = NULL, **envp = NULL;
	DockbarxAppletPrivate *priv = applet->priv;

	envp = g_get_environ ();
	g_shell_parse_argv ("/usr/bin/pkill -f 'python.*xfce4-dockbarx-plug'", NULL, &argv, NULL);

	if (g_spawn_async (NULL, argv, envp, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL)) {
		g_child_watch_add (pid, (GChildWatchFunc) kill_dockbarx_done_cb, applet);
	} else {
		g_idle_add ((GSourceFunc)start_dockbarx, applet);
	}

	g_strfreev (argv);
}

static gboolean
restart_dockbarx_idle (gpointer data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	kill_dockbarx (applet);

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	return FALSE;
}

static void
update_launchers (void)
{
	g_spawn_command_line_sync (GOOROOM_UPDATE_LAUNCHERS_HELPER, NULL, NULL, NULL, NULL);
}

static void
init_thread_done_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (user_data);
	DockbarxAppletPrivate *priv = applet->priv;

	g_idle_add ((GSourceFunc)start_dockbarx, applet);
}

static void
start_init_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
	update_launchers ();

	/* The task has finished */
	g_task_return_boolean (task, TRUE);
}


static void
handle_method_call (GDBusConnection *conn,
                    const gchar *sender,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	if (!g_strcmp0 (method_name, "Restart")) {
		if (priv->timeout_id == 0)
			priv->timeout_id = g_idle_add ((GSourceFunc)restart_dockbarx_idle, applet);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
	} else {
		g_dbus_method_invocation_return_error (invocation,
                                               G_DBUS_ERROR,
                                               G_DBUS_ERROR_FAILED,
                                               "No such method: %s", method_name);
	}
}

static gboolean
set_max_size_cb (gpointer data)
{
	gint size;
	GtkAllocation alloc;
	GtkOrientation orientation;

	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	orientation = gp_applet_get_orientation (GP_APPLET (applet));
	gtk_widget_get_allocation (GTK_WIDGET (applet), &alloc);

	size = (orientation == GTK_ORIENTATION_HORIZONTAL) ? alloc.width : alloc.height;

	g_settings_set_int (priv->dockbarx_settings, "max-size", size);

	return FALSE;
}

static void
dockbarx_applet_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
	GpApplet *gp_applet = GP_APPLET (widget);

	GTK_WIDGET_CLASS (dockbarx_applet_parent_class)->size_allocate (widget, allocation);

	gint size_hints[2];
	size_hints[0] = 32767;
	size_hints[1] = 0;

	gp_applet_set_size_hints (gp_applet, size_hints, 2, 0);

	g_timeout_add (100, (GSourceFunc)set_max_size_cb, gp_applet);
}

static void
monitors_changed_cb (GdkScreen *screen, gpointer data)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (data);
	DockbarxAppletPrivate *priv = applet->priv;

	if (priv->timeout_id == 0)
		priv->timeout_id = g_idle_add ((GSourceFunc)restart_dockbarx_idle, applet);
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

	if (priv->owner_id) {
		g_bus_unown_name (priv->owner_id);
		priv->owner_id = 0;
	}

	if (priv->reg_id) {
		g_dbus_connection_unregister_object (priv->connection, priv->reg_id);
		priv->reg_id = 0;
	}

	if (priv->dockbarx_settings)
		g_object_unref (priv->dockbarx_settings);

	if (G_OBJECT_CLASS (dockbarx_applet_parent_class)->finalize)
		G_OBJECT_CLASS (dockbarx_applet_parent_class)->finalize (object);
}

static gboolean
dockbarx_applet_fill (DockbarxApplet *applet)
{
	GTask *task;

	task = g_task_new (NULL, NULL, init_thread_done_cb, applet);
	g_task_run_in_thread (task, start_init_thread);
	g_object_unref (task);

	return TRUE;
}

static void
dockbarx_applet_constructed (GObject *object)
{
	DockbarxApplet *applet = DOCKBARX_APPLET (object);

	dockbarx_applet_fill (DOCKBARX_APPLET (applet));
}

static void
dockbarx_applet_init (DockbarxApplet *applet)
{
	GdkScreen *screen;
	GSettingsSchema *schema = NULL;

	DockbarxAppletPrivate *priv;

	priv = applet->priv = dockbarx_applet_get_instance_private (applet);

	gp_applet_set_flags (GP_APPLET (applet),
                         GP_APPLET_FLAGS_EXPAND_MAJOR |
                         GP_APPLET_FLAGS_EXPAND_MINOR);

	schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                              "org.dockbarx", TRUE);
	if (schema) {
		priv->dockbarx_settings = g_settings_new_full (schema, NULL, NULL);
		g_settings_schema_unref (schema);
	}

	priv->socket     = NULL;
	priv->reg_id     = 0;
	priv->owner_id   = 0;
	priv->timeout_id = 0;

	screen = gdk_screen_get_default ();

	gtk_widget_show_all (GTK_WIDGET (applet));

	g_signal_connect (screen, "monitors-changed",
                      G_CALLBACK (monitors_changed_cb), applet);
}

static void
dockbarx_applet_class_init (DockbarxAppletClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	object_class->finalize = dockbarx_applet_finalize;
	object_class->constructed = dockbarx_applet_constructed;
	widget_class->size_allocate = dockbarx_applet_size_allocate;
}
