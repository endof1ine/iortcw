#!/usr/bin/env python3
"""Writes the Switch's controller menus (romfs/main/ui/switch_*.menu): the
same rows for the main menu's and the in-game Controls, which these replace
on the Switch (ui_main.c's UI_SwitchMenus). Run after editing ROWS."""

import os

# (label, cvar, kind, values): kind "slider" takes (default, min, max),
# "multi" a list of (label, value) with the default first, "yesno" nothing.
# A None row is a gap between sections.
ROWS = [
    ("Look Speed, Horizontal:", "j_yawSpeed", "slider", (180, 90, 360)),
    ("Look Speed, Vertical:", "j_pitchSpeed", "slider", (100, 50, 200)),
    ("Invert Look:", "j_pitch", "multi", [("No", 0.022), ("Yes", -0.022)]),
    ("Look Response:", "j_lookCurve", "multi", [("Smooth", 2), ("Linear", 1), ("Precise", 3)]),
    ("Turn Boost:", "j_lookBoost", "multi", [("On", 1.5), ("Off", 1)]),
    ("Stick Deadzone:", "joy_threshold", "multi", [("Normal", 0.15), ("Small", 0.08), ("Large", 0.25)]),
    None,
    ("Aim Slowdown:", "cg_aimFriction", "multi", [("Normal", 0.45), ("Strong", 0.3), ("Light", 0.65), ("Off", 0)]),
    ("Aim Magnetism:", "cg_aimMagnetism", "multi", [("Normal", 0.6), ("Strong", 0.85), ("Light", 0.35), ("Off", 0)]),
    ("Bullet Magnetism:", "g_aimAssist", "yesno", None),
    None,
    ("Vibration:", "in_rumble", "multi", [("Normal", 1), ("Strong", 1.5), ("Light", 0.5), ("Off", 0)]),
]

MENUS = {
    # the main menu's, beside the setup menu's tabs
    "switch_controls.menu": ("control_menu", """	onEsc {
		close control_menu ;
		close setup_menu ;
		open main
	}
"""),
    # the in-game menu's
    "switch_ingame_controls.menu": ("ingame_controls", """	outOfBoundsClick
"""),
}


def row(label, cvar, kind, values, y):
    if kind == "slider":
        kind_lines = "\t\ttype ITEM_TYPE_SLIDER\n\t\tcvarfloat \"%s\" %g %g %g\n" % ((cvar,) + values)
    elif kind == "multi":
        pairs = " ".join('"%s" %g' % pair for pair in values)
        kind_lines = "\t\ttype ITEM_TYPE_MULTI\n\t\tcvar \"%s\"\n\t\tcvarFloatList { %s }\n" % (cvar, pairs)
    else:
        kind_lines = "\t\ttype ITEM_TYPE_YESNO\n\t\tcvar \"%s\"\n" % cvar
    return """	itemDef {
		name setting
		text "%s"
%s		rect 82 %d 290 12
		textalign ITEM_ALIGN_RIGHT
		textalignx 142
		textaligny 10
		textscale .23
		style WINDOW_STYLE_FILLED
		backcolor 1 1 1 .07
		forecolor 1 1 1 1
		visible 1
	}
""" % (label, kind_lines, y)


def menu(name, extra):
    rows, y = [], 32
    for r in ROWS:
        if r is None:
            y += 8
            continue
        rows.append(row(*r, y))
        y += 15
    return """// Written by gen_menus.py: the Switch's controller settings
#include "ui/menudef.h"

{
menuDef {
	name "%s"
	visible 0
	fullscreen 0
	rect 100 125 443 340
	focusColor 1 .75 0 1
	style 1
	border 1
%s
	itemDef {
		name window
		rect 0 2 443 300
		style WINDOW_STYLE_FILLED
		border 1
		bordercolor .5 .5 .5 .5
		forecolor 1 1 1 1
		backcolor 0 0 0 .25
		visible 1
		decoration
	}
	itemDef {
		name title
		text "Controller"
		rect 2 4 439 20
		style WINDOW_STYLE_FILLED
		border 1
		bordercolor .1 .1 .1 .2
		backcolor .3 0.5 0.2 .25
		textalign ITEM_ALIGN_CENTER
		textalignx 219
		textaligny 14
		textscale .23
		forecolor 1 1 1 1
		visible 1
		decoration
	}
%s}
}
""" % (name, extra, "".join(rows))


here = os.path.dirname(os.path.abspath(__file__))
for filename, (name, extra) in MENUS.items():
    with open(os.path.join(here, "romfs", "main", "ui", filename), "w") as f:
        f.write(menu(name, extra))
