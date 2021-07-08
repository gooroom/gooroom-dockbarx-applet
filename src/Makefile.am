dockbarx_plugdir = $(GNOME_PANEL_MODULES_DIR)
dockbarx_plug_SCRIPTS = xfce4-dockbarx-plug.py

gooroomappletdir = $(GNOME_PANEL_MODULES_DIR)
gooroomapplet_LTLIBRARIES = libgooroom-dockbarx-applet.la

libgooroom_dockbarx_applet_la_CPPFLAGS = \
	-I$(srcdir) \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOMELOCALEDIR=\""$(localedir)"\" \
	-DDOCKBARX_PLUG=\"$(dockbarx_plugdir)/xfce4-dockbarx-plug.py\" \
	-DGOOROOM_UPDATE_LAUNCHERS_HELPER=\"$(dockbarx_plugdir)/gooroom-update-launchers-helper\"

libgooroom_dockbarx_applet_la_CFLAGS = \
	$(LIBGNOMEPANEL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(JSON_C_CFLAGS) \
	$(AM_CFLAGS)

libgooroom_dockbarx_applet_la_SOURCES = \
	panel-glib.h \
	panel-glib.c \
	dockbarx-applet.c \
	dockbarx-applet.h \
	dockbarx-applet-module.c

libgooroom_dockbarx_applet_la_LDFLAGS = \
	-avoid-version \
	$(AM_LDFLAGS)

libgooroom_dockbarx_applet_la_LIBADD = \
	$(LIBGNOMEPANEL_LIBS) \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(JSON_C_LIBS)

pkglibexec_PROGRAMS = \
    gooroom-update-launchers-helper

gooroom_update_launchers_helper_SOURCES = \
	panel-glib.c \
	gooroom-update-launchers-helper.c

gooroom_update_launchers_helper_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_C_CFLAGS) \
	$(GIO_UNIX_CFLAGS)

gooroom_update_launchers_helper_LDFLAGS = \
	$(GLIB_LIBS) \
	$(JSON_C_LIBS) \
	$(GIO_UNIX_LIBS)
