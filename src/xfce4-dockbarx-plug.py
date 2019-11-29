#!/usr/bin/python2
#
#   xfce4-dockbarx-plug
#
#   
#   Copyright 2008-2013
#      Aleksey Shaferov, Matias Sars, and Trent McPheron
#
#   Copyright (C) 2018-2019 Gooroom <gooroom@gooroom.kr>
#
#   DockbarX is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   DockbarX is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with dockbar.  If not, see <http://www.gnu.org/licenses/>.

import sys
import io
import traceback
import os

import gi
gi.require_version("Gtk", "3.0")
gi.require_version("Gio", "2.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gtk
from gi.repository import Gio
from gi.repository import Gdk
import cairo

from optparse import OptionParser


GSETTINGS_CLIENT = Gio.Settings.new("org.dockbarx")
GSETTINGS_DT_IFACE_CLIENT = Gio.Settings.new("org.gnome.desktop.interface")
#BACKGROUND_PATH = "/usr/share/backgrounds/gooroom/panel-bg.png"

# A very minimal plug application that loads DockbarX
# so that the embed plugin can, well, embed it.
class DockBarXFCEPlug(Gtk.Plug):
    # We want to do our own expose instead of the default.
    __gsignals__ = {"draw": "override"}

    def __init__ (self):
        import dockbarx.dockbar as db

        parser = OptionParser()
        parser.add_option("-s", "--socket", default = 0, help = "Socket ID")
        (options, args) = parser.parse_args()

        # Sanity checks.
        if options.socket == 0:
            sys.exit("This program needs to be run by the XFCE DBX plugin.")
        
        Gtk.Plug.__init__(self)
        self.construct(int(options.socket))
        self.connect("destroy", self.destroy)
        self.set_app_paintable(True)
        gtk_screen = Gdk.Screen.get_default()
        colormap = gtk_screen.get_rgba_visual()
        # don't use this function after pygobject
        #if colormap is None: colormap = gtk_screen.get_rgb_colormap()
        self.set_visual(colormap)

        GSETTINGS_CLIENT.connect("changed", self.on_max_size_changed)
        GSETTINGS_DT_IFACE_CLIENT.connect("changed", self.on_icon_theme_changed)

#        self.pattern = None
#        if os.path.exists(BACKGROUND_PATH):
#            surface = cairo.ImageSurface.create_from_png(BACKGROUND_PATH)
#            self.pattern = cairo.SurfacePattern(surface)
#            self.pattern.set_extend(cairo.EXTEND_REPEAT)

        self.dockbar = db.DockBar(self)
        self.dockbar.set_expose_on_clear(True)
        self.dockbar.load()
        self.align = Gtk.Alignment()
        self.align.set(0.0, 0.5, 1, 1)
        Gtk.Alignment.set_padding(self.align, 2, 0, 0, 0)
        self.align.add(self.dockbar.get_container())
        self.add(self.align)
        self.dockbar.set_max_size(self.get_size())
        self.show_all()

    def on_max_size_changed(self, settings, keyname):
        if keyname == 'max-size':
            self.dockbar.set_max_size(settings.get_int(keyname))

    def on_icon_theme_changed(self, settings, keyname):
        if keyname == 'icon-theme':
            self.dockbar.reload()

    def get_size (self):
        max_size = GSETTINGS_CLIENT.get_int("max-size")
        if max_size < 1: max_size = 32767
        return max_size
    
    def readd_container (self, container):
        self.align.add(container)
        self.dockbar.set_max_size(self.get_size())
        self.show_all()

    # Imitates gnome-panel's expose event.
    def do_draw(self, event):
        # don't use this function after 3.22
        ctx = self.get_window().cairo_create()
        ctx.set_antialias(cairo.ANTIALIAS_NONE)
        ctx.set_operator(cairo.OPERATOR_SOURCE)
        rect = self.get_allocation()
        ctx.rectangle(rect.x, rect.y,
                      rect.width, rect.height)
        ctx.clip()
        ctx.set_source_rgba(0.0,0.0,0.0,0.8)
        ctx.paint()
        if self.get_child():
            self.propagate_draw(self.get_child(), event)

    def destroy (self, widget, data=None):
        Gtk.main_quit()


if __name__ == '__main__':
    dbx = DockBarXFCEPlug()
    Gtk.main()
