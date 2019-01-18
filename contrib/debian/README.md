
Debian
====================
This directory contains files used to package verged/verge-qt
for Debian-based Linux systems. If you compile verged/verge-qt yourself, there are some useful files here.

## verge: URI support ##


verge-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install verge-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your verge-qt binary to `/usr/bin`
and the `../../share/pixmaps/verge128.png` to `/usr/share/pixmaps`

verge-qt.protocol (KDE)

