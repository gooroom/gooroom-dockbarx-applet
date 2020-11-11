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

#ifndef __DOCKBARX_APPLET_H__
#define __DOCKBARX_APPLET_H__

G_BEGIN_DECLS

#include <libgnome-panel/gp-applet.h>

#define DOCKBARX_TYPE_APPLET           (dockbarx_applet_get_type ())
#define DOCKBARX_APPLET(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), DOCKBARX_TYPE_APPLET, DockbarxApplet))
#define DOCKBARX_APPLET_CLASS(obj)     (G_TYPE_CHECK_CLASS_CAST    ((obj), DOCKBARX_TYPE_APPLET, DockbarxAppletClass))
#define DOCKBARX_IS_APPLET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DOCKBARX_TYPE_APPLET))
#define DOCKBARX_IS_APPLET_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE    ((obj), DOCKBARX_TYPE_APPLET))
#define DOCKBARX_APPLET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), DOCKBARX_TYPE_APPLET, DockbarxAppletClass))

typedef struct _DockbarxApplet        DockbarxApplet;
typedef struct _DockbarxAppletClass   DockbarxAppletClass;
typedef struct _DockbarxAppletPrivate DockbarxAppletPrivate;

struct _DockbarxApplet {
	GpApplet 	           parent;
	DockbarxAppletPrivate *priv;
};

struct _DockbarxAppletClass {
	GpAppletClass parent_class;
};

GType dockbarx_applet_get_type (void);

G_END_DECLS

#endif /* __DOCKBARX_APPLET_H__*/
