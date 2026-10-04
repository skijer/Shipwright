"""Shared GI/held lantern: open metal cage, glass chamber and explicit grip."""
import math
import numpy as np
from meshkit import Model, TAU


def build():
    m = Model('lantern', 'Lantern', 'objects/nei_gi_redesign/lantern/gi_dl', .025, .5)
    m.material('brass', [.62, .37, .12], 'metal', .78, .36)
    m.material('polished_brass', [.94, .70, .31], 'metal', .82, .28)
    m.material('dark_bronze', [.17, .13, .085], 'metal', .7, .52)
    m.material('roof_patina', [.18, .27, .24], 'metal', .65, .58)
    m.material('wick', [.055, .038, .024], 'braid', 0, .9)
    m.material('glass', [.65, .84, .86], None, .05, .18, alpha=.14)

    # A broad weighted foot and slender rim give the flame a clear floor.
    m.lathe('Weighted hexagonal foot', 'dark_bronze',
            [(-36, 17), (-35, 23), (-33, 26), (-30, 26), (-28, 22)], 12)
    m.lathe('Foot brass shoulder', 'brass', [(-34, 24), (-33, 26.5), (-31.5, 26.5), (-30.5, 24)], 24)
    m.lathe('Foot polished edge', 'polished_brass', [(-33.2, 26.1), (-32.8, 27), (-32.2, 27), (-31.8, 26.1)], 24)
    m.lathe('Chamber lower sill', 'brass', [(-29, 23), (-28, 24), (-26.5, 24), (-25.8, 21)], 24)
    m.lathe('Interior burner cup', 'dark_bronze', [(-29, 7), (-27.7, 8), (-26.8, 5)], 16)
    m.lathe('Wick brass ferrule', 'polished_brass', [(-27, 2.7), (-25.6, 2.7)], 12)
    m.tube('Visible unlit wick', 'wick', [[0, -26.5, 0], [.25, -24.5, .1], [-.25, -23.8, .15]], 1.1, 7)

    # Four bowed ribs leave the chamber legible from front, back and three-quarter views.
    for j in range(4):
        a = TAU * j / 4 + math.pi / 4
        ys = np.linspace(-28, 28, 17)
        radius = 22.6 + 2.0 * np.sin(np.linspace(0, math.pi, len(ys)))
        path = np.c_[radius * math.cos(a), ys, radius * math.sin(a)]
        m.tube('Bowed cage rib %d' % j, 'brass', path, 1.45, 7)
        for y in (-26.5, 27):
            m.sphere('Cage rivet', 'polished_brass',
                     [23.8 * math.cos(a), y, 23.8 * math.sin(a)], [2.0, 2.0, 2.0], 4, 8)

    # The thin pane is a genuine transparent sidewall, with no opaque inner core.
    m.lathe('Pale glass chamber', 'glass', [(-26.5, 20.5), (25.5, 20.5)], 24, cap=False)
    m.lathe('Upper chamber rim', 'brass', [(25.5, 21), (26.5, 24), (28.5, 24), (29.2, 22)], 24)
    m.lathe('Upper polished bead', 'polished_brass', [(27, 24), (27.6, 24.8), (28.2, 24)], 24)

    # Layered vented canopy: the dark recessed waist separates cap and body.
    m.lathe('Canopy lip', 'dark_bronze', [(28, 24), (29, 27), (30.5, 27), (31, 23)], 24)
    m.lathe('Patinated canopy', 'roof_patina', [(30, 25), (32, 23), (35, 17), (37, 9)], 24)
    m.lathe('Canopy brass rim', 'polished_brass', [(30, 25), (30.5, 25.6), (31, 25)], 24)
    m.lathe('Canopy vent recess', 'dark_bronze', [(36, 10), (39, 10)], 16)
    m.lathe('Vent crown', 'brass', [(38.7, 11), (40, 10), (40.6, 6)], 16)
    for j in range(8):
        a = TAU * j / 8
        m.tube('Vent separator', 'polished_brass',
               [[10 * math.cos(a), 36.3, 10 * math.sin(a)],
                [10 * math.cos(a), 38.9, 10 * math.sin(a)]], .55, 5)

    # The hand meets the top center of the loop at y=52; the chamber stays y=0.
    a = np.linspace(0, TAU, 41)
    path = np.c_[8.5 * np.cos(a), 41.5 + 10.5 * np.sin(a), np.zeros(len(a))]
    m.tube('Forged carrying loop', 'dark_bronze', path, 1.4, 8, cap=False)
    for x in (-7.2, 7.2):
        m.sphere('Handle hinge', 'polished_brass', [x, 36, 0], [2.4, 2.4, 2.4], 5, 10)
    m.markers.update(grip_author=[0, 52, 0], flame_author=[0, 0, 0],
                     chamber_radius_author=20.5, chamber_y_author=[-26.5, 25.5],
                     held_author_scale=7 / 52, held_flame_offset_world=[0, -7, 0])
    m.notes = [
        'Shared upright cage, glass and unlit wick for GI and held rendering; no gameplay resources replaced.',
        'GI scale .025, effective author scale .5, native matrix 1.25; held uses 7/52 world units per author unit.',
        'Grip at author y=52 puts chamber center exactly seven world units below the hand.',
        'Opaque cage first, contained deterministic effects second, transparent glass last.',
    ]
    return m


if __name__ == '__main__':
    from meshkit import export_resources
    from preview import checkpoint
    model = build()
    checkpoint(model, export_resources(model))
