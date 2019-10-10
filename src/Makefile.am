dockbarx_plugdir = $(pkglibdir)
dockbarx_plug_SCRIPTS = xfce4-dockbarx-plug.py

gooroomappletdir = $(pkglibdir)
gooroomapplet_LTLIBRARIES = libgooroom-dockbarx-applet.la

libgooroom_dockbarx_applet_la_CPPFLAGS = \
	-I$(srcdir) \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOMELOCALEDIR=\""$(localedir)"\"	\
	-DDOCKBARX_PLUG=\"$(dockbarx_plugdir)/xfce4-dockbarx-plug.py\"

libgooroom_dockbarx_applet_la_CFLAGS = \
	$(GNOME_APPLETS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(JSON_C_CFLAGS) \
	$(AM_CFLAGS)

libgooroom_dockbarx_applet_la_SOURCES = \
	panel-glib.h \
	panel-glib.c \
	dockbarx-applet.c \
	dockbarx-applet.h

libgooroom_dockbarx_applet_la_LDFLAGS = \
	-avoid-version \
	$(AM_LDFLAGS)

libgooroom_dockbarx_applet_la_LIBADD = \
	$(GNOME_APPLETS_LIBS) \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(JSON_C_LIBS)
