"""Four missing vanilla NEI presentations, based on the supplied original icons.

This module is deliberately separate from the accepted 17 GI builders. Geometry
uses the same quantized mesh/resource/GLB pipeline; no existing model is rebuilt.
"""
import math
import numpy as np
from meshkit import Model, TAU, bezier, rotation, unit


def _model(slug, name, draw_scale, effective=.5):
    return Model(slug, name, 'objects/nei_gi_redesign/' + slug + '/gi_dl',
                 draw_scale, effective)


def _cloth(color, size=256):
    """Quiet woven cloth; folds are geometry, not painted fake relief."""
    y, x = np.mgrid[0:size, 0:size]
    weave = .92 + .028 * np.cos(x * math.pi) + .025 * np.cos(y * math.pi)
    grain = np.random.default_rng(918).normal(0, .009, (size, size))
    rgb = np.clip((weave + grain)[..., None] * np.array(color) * 255, 0, 255).astype(np.uint8)
    return np.concatenate([rgb, np.full((size, size, 1), 255, np.uint8)], axis=2)


def _radial_box(m, name, mat, angle, radial, width, height, depth, y=0):
    start = len(m.parts)
    m.polygon(name, mat, [(-width/2, -height/2), (width/2, -height/2),
                         (width/2, height/2), (-width/2, height/2)],
              depth=depth, z=radial, bevel=min(width, height, depth) * .16)
    m.transform(rotation('y', angle), offset=[0, y, 0], start=start)


def spinner():
    m = _model('spinner', 'Spinner', .3)
    m.material('bronze', [.45, .27, .14], 'metal', .75, .42)
    m.material('polished_bronze', [.72, .49, .25], 'metal', .82, .30)
    m.material('old_bronze', [.24, .18, .12], 'metal', .7, .57)
    m.material('stone_teeth', [.57, .56, .43], 'stone', .28, .78)
    m.material('tooth_edge', [.71, .70, .57], 'stone', .25, .62)
    m.material('navy_enamel', [.085, .17, .23], 'ceramic', .45, .34)
    m.material('sun_gold', [.90, .76, .32], 'metal', .72, .28)
    m.material('inset_dark', [.105, .105, .085], 'stone', 0, .9)

    # The supplied icon's broad, flat sun dial and stone gear teeth are retained.
    m.lathe('Conical bronze underside', 'old_bronze',
            [(-19, 2.5), (-17, 6), (-13, 10), (-10, 18), (-6, 27), (-2, 36), (2, 39)], 40)
    m.lathe('Lower spindle collar', 'bronze', [(-18, 4), (-17, 7), (-14.5, 8), (-13, 7)], 16)
    m.lathe('Spindle polished lip', 'polished_bronze', [(-17, 6.5), (-16.5, 8), (-15.5, 8), (-15, 7)], 20)
    m.lathe('Broad gear housing', 'bronze', [(-3, 35), (-1, 40), (6.5, 40), (9, 37)], 40)
    m.lathe('Housing upper shoulder', 'polished_bronze', [(6, 38), (8, 41), (9.5, 40), (11, 35)], 40)
    m.lathe('Recessed blue sun dial', 'navy_enamel', [(9.5, 38.8), (11.5, 39.6), (12, 39.3)], 40)
    m.ring('Outer deck rolled rim', 'polished_bronze', [0, 11.3, 0], 40.2, .8, 'y', 48, 6)
    m.ring('Inner deck gold circle', 'sun_gold', [0, 12.03, 0], 33.8, .15, 'y', 48, 5)
    m.ring('Sun medallion border', 'bronze', [0, 12.02, 0], 12.4, .18, 'y', 32, 5)

    # Ten broad stone blocks, each with the original inset round face bolt.
    for j in range(10):
        deg = 360 * j / 10
        _radial_box(m, 'Stone gear tooth %02d' % j, 'stone_teeth', deg,
                    42.5, 10.5, 11.5, 10.5, y=1.2)
        start = len(m.parts)
        m.ring('Inset circular stone boss', 'old_bronze', [0, 1.2, 48], 3.35, .72,
               segments=14, sides=5)
        m.sphere('Stone boss center', 'tooth_edge', [0, 1.2, 48.1], [2.6, 2.6, .7], 5, 12)
        m.transform(rotation('y', deg), start=start)
        a = math.radians(deg)
        radii = np.array([7.3, 12, 20, 28, 35.5])
        ys = np.array([-14.2, -11.4, -8, -4.9, -.7])
        m.tube('Underside cast rib', 'bronze',
               np.c_[np.sin(a)*radii, ys, np.cos(a)*radii], [.65, 1.2, 1.4, 1.45, 1.5], 6)
    for j in range(40):
        _radial_box(m, 'Knurled outer deck edge', 'old_bronze', 360*j/40,
                    39.5, 1.05, 3.1, 1.2, y=8.2)

    # Exact geometric sunburst, visible from above and between Link's boots.
    xy = []
    for j in range(24):
        a = TAU * j / 24
        r = 11.4 if j % 2 == 0 else 4.3
        xy.append((math.cos(a)*r, math.sin(a)*r))
    start = len(m.parts)
    m.polygon('Twelve ray gold sun', 'sun_gold', xy, depth=.30, bevel=.10)
    m.transform(rotation('x', 90), offset=[0, 12.08, 0], start=start)
    for j in range(12):
        a = TAU * j / 12
        p = np.array([math.sin(a), 0, math.cos(a)])
        m.tube('Dial radial gold index', 'sun_gold', [p*29.8+[0, 12.03, 0], p*32+[0, 12.03, 0]], .12, 4)
    m.markers = dict(deck_author=[0, 12, 0], deck_radius_author=39.3,
                     outer_radius_author=48.8, underside_author=[0, -19, 0],
                     gameplay_world_scale=.5, gameplay_deck_world_y=-.5625,
                     gameplay_radius_world=24.4, gameplay_lowest_world_y=-14)
    m.notes = ['Supplied Spinner icon: bronze sun dial, ten stone gear blocks, conical spindle.',
               'GI caller .3; effective .5. Gameplay uses the same deck art, with sole-height fitting and a shortened underside.',
               'Ride player origin floor+17; no gameplay physics/collider changes.']
    return m


def cane_of_somaria(display_pose=True):
    m = _model('cane_of_somaria', 'Cane of Somaria', .25)
    m.material('lacquer_red', [.75, .105, .055], 'ceramic', .35, .27)
    m.material('warm_red', [.93, .25, .065], 'metal', .5, .32)
    m.material('deep_red', [.29, .035, .025], 'leather', 0, .65)
    m.material('gold', [.94, .61, .16], 'metal', .75, .3)
    m.material('ruby', [.88, .045, .065], None, .5, .24)
    m.material('ruby_light', [1., .23, .18], None, .45, .25)

    m.tube('Slender red staff', 'lacquer_red', [[0, -49, 0], [0, -35, 0], [0, -15, 0],
              [0, 5, 0], [0, 17, 0]], [1.6, 1.9, 2.1, 2.1, 2.45], 12)
    m.tube('Dark red hand grip', 'deep_red', [[0, -29, 0], [0, -27, 0], [0, -18, 0], [0, -16, 0]],
           [2.0, 2.35, 2.35, 2.05], 12)
    for y in [-29.2, -16, 11.5]:
        m.lathe('Gold shaft ferrule', 'gold', [(y-.85, 2.05), (y-.6, 2.7), (y+.6, 2.7), (y+.85, 2.05)], 12)
    for y in np.linspace(-26.6, -18.4, 5):
        m.ring('Grip subtle winding', 'lacquer_red', [0, y, 0], 2.32, .28, 'y', 12, 4)
    m.lathe('Golden foot cap', 'gold', [(-50, 1.3), (-49.3, 2), (-47.8, 2), (-47, 1.7)], 12)

    # The original icon is a crook surrounding a suspended red faceted stone.
    path = np.concatenate([
        bezier([0, 15, 0], [0, 23, 0], [1, 34, 0], [-3, 41, 0], 12),
        bezier([-3, 41, 0], [-8, 51, 0], [-22, 48, 0], [-23, 38, 0], 14)[1:],
        bezier([-23, 38, 0], [-25, 28, 0], [-18, 25, 0], [-14, 28, 0], 12)[1:],
    ])
    m.tube('Continuous red Somaria crook', 'warm_red', path,
           np.linspace(2.65, 3.0, len(path)), 12)
    # A narrow gold edge traces the warm, angular highlights in the old icon.
    m.tube('Fine gold crook front edge', 'gold', path[5:]+[0, 0, 2.65], .4, 5)
    m.tube('Fine gold crook back edge', 'gold', path[5:]+[0, 0, -2.65], .4, 5)
    m.tube('Ruby suspension prong', 'gold', [[-10.5, 44, 0], [-11.2, 40.8, 0]], .65, 6)
    m.crystal('Suspended red Somaria ruby', 'ruby', [-11.5, 36.7, 0], 4.6, 11, 6)
    m.crystal('Ruby bright front facet', 'ruby_light', [-11.5, 36.7, 2.2], 2.1, 6.7, 4)
    m.sphere('Hook terminal gold bead', 'gold', path[-1], [3.05, 3.05, 3.05], 5, 10)
    m.markers = dict(grip_author=[0, -22.5, 0], tip_author=[-11.5, 36.7, 0],
                     gameplay_world_scale=.35, canonical_shaft_axis=[0, 1, 0])
    if display_pose:
        m.transform(rotation('z', -22), offset=[5, 2, 0])
        for key in ('grip_author', 'tip_author'):
            m.markers[key] = (rotation('z', -22) @ m.markers[key] + [5, 2, 0]).tolist()
    m.notes = ['Supplied icon: orange-red hooked cane encircling a red faceted ruby; restrained gold fittings.',
               'Only Somaria uses this replacement. Pacci, Byrna, Trirod and Ultrahand resources stay unchanged.',
               'Canonical grip author y=-22.5; gameplay is derived before the GI display lean.']
    return m


def _bent_cap(m):
    # Rings perpendicular to the curved crown axis produce a folded cloth cone.
    key_centers = np.array([[0,-5,0], [0,2,0], [1,11,0], [3,20,0], [8,29,0],
                        [14,37,0], [21,44,0], [29,49,0], [36,50,0]])
    key_radii = np.array([[28,22], [27,21], [25,19], [22,16.5], [18,13],
                      [13.5,10], [9,6.8], [5.4,4.2], [2,1.8]])
    ts=np.linspace(0,1,33);ks=np.linspace(0,1,len(key_centers))
    centers=np.stack([np.interp(ts,ks,key_centers[:,i]) for i in range(3)],axis=1)
    radii=np.stack([np.interp(ts,ks,key_radii[:,i]) for i in range(2)],axis=1)
    tangents = unit(np.gradient(centers, axis=0))
    verts=[]; norms=[]; uvs=[]; faces=[]; sides=40
    for i,c in enumerate(centers):
        across = unit(np.cross(tangents[i], [0,0,1]))
        for j in range(sides+1):
            a=TAU*j/sides
            t=i/(len(centers)-1)
            ripple=(math.sin(t*TAU*4.4+a*2.1)*1.45 + math.sin(t*TAU*6.8-a*1.4)*.5)
            ripple*=math.sin(math.pi*t)**.7*min(radii[i,0]/17,1)
            radial=across*math.cos(a)*(radii[i,0]+ripple)+np.array([0,0,1])*math.sin(a)*(radii[i,1]+ripple)
            verts.append(c+radial);norms.append(unit(radial));uvs.append([j/sides,i/(len(centers)-1)])
    for i in range(len(centers)-1):
        for j in range(sides):
            a=i*(sides+1)+j;b=a+sides+1;faces += [[a,b,a+1],[a+1,b,b+1]]
    grid=np.array(verts).reshape(len(centers),sides+1,3)
    calculated=unit(np.cross(np.gradient(grid,axis=0),np.gradient(grid,axis=1)))
    if np.mean(np.sum(calculated.reshape(-1,3)*np.array(norms),axis=1))<0:calculated=-calculated
    m.add('Soft bent crimson crown', 'crimson_cloth', verts, calculated.reshape(-1,3), uvs, faces)
    return centers[-1]


def minish_cap():
    m = _model('minish_cap', 'Minish Cap', .5, .6)
    m.material('crimson_cloth', [.57,.018,.027], tex=_cloth([.83,.025,.04]), rough=.68)
    m.material('crimson_highlight', [.76,.042,.045], tex=_cloth([.77,.042,.045]), rough=.6)
    m.material('dark_lining', [.105,.024,.035], tex=_cloth([.16,.027,.036]), rough=.9)
    m.material('antique_gold', [.62,.36,.065], 'metal', .8, .43)
    m.material('polished_gold', [.94,.70,.20], 'metal', .8, .27)
    m.material('garnet', [.70,.008,.025], None, .55, .21)
    m.material('ruby_glint', [1,.10,.07], None, .6, .2)
    tip=_bent_cap(m)

    # An open brim and inner lining, not a filled disk through the hat opening.
    for mat, rings, name in [
        ('dark_lining', [(-14,26),(-7,26),(-3,25)], 'Visible dark inner brim'),
        ('antique_gold', [(-14,28.4),(-12.5,30.6),(-5.3,30.6),(-3.5,28.5)], 'Wide gold brocade band'),
    ]:
        start=len(m.parts);m.lathe(name,mat,rings,40,cap=False);m.transform(scale=[1,1,.78],start=start)
    for y in [-13.2,-4.5]:
        m.ring('Rolled gold brim edging','polished_gold',[0,y,0],[30.3,23.6],.85,'y',48,6)
    # Short projecting front brim follows the red-and-gold reference silhouette.
    a=np.linspace(-math.pi*.48,math.pi*.48,25)
    inner=np.c_[np.sin(a)*28,np.full(len(a),-13),np.cos(a)*22]
    outer=np.c_[np.sin(a)*33,-14.5-2*np.cos(a),np.cos(a)*34]
    p=np.vstack([inner,outer]);n=np.tile([0,1,0],(len(p),1));uv=np.c_[p[:,0]/70+.5,p[:,2]/70+.5]
    tr=[]
    for j in range(len(a)-1):tr.extend([[j,j+1,j+len(a)],[j+1,j+len(a)+1,j+len(a)]])
    m.add('Crimson front brim','crimson_cloth',p,n,uv,tr)
    m.add('Brim underside','dark_lining',p-[0,.6,0],-n,uv,np.array(tr)[:,::-1])
    m.tube('Gold front brim piping','polished_gold',outer,.55,6)
    # Brocade loops and faceted rubies across the hat band.
    for j in range(12):
        a=TAU*j/12
        center=np.array([30.65*math.sin(a),-8.8,23.85*math.cos(a)])
        start=len(m.parts)
        m.ring('Brocade loop','polished_gold',[0,-8.8,0],[2.0,2.7],.35,segments=16,sides=4)
        m.transform(rotation('y',math.degrees(a)),offset=[center[0],0,center[2]],start=start)
    for deg in [-62,0,62,180]:
        a=math.radians(deg);center=np.array([31*math.sin(a),-8.4,24.2*math.cos(a)])
        start=len(m.parts)
        m.sphere('Raised gold ruby setting','polished_gold',[0,0,0],[3.7,4.5,1.6],6,12)
        m.crystal('Brim red jewel','garnet',[0,0,1.5],2.8,7.5,6)
        m.transform(rotation('y',deg),offset=center,start=start)
    m.sphere('Cone terminal gold mount','antique_gold',tip,[3.1,3.1,3.1],6,12)
    m.sphere('Crimson orb at cap tip','garnet',tip+[3.5,1,0],[5.5,5.5,5.5],8,16)
    m.sphere('Small orb light facet','ruby_glint',tip+[5.0,3.0,3.7],[1.25,1.5,.55],4,8)
    # The long red streamer is visible beside the cone, with gold tassels at its tip.
    path=bezier(tip+[1,-2,0],[42,22,0],[36,2,0],[33,-11,0],24)
    m.tube('Red hanging cap streamer','crimson_cloth',path,np.linspace(1.7,2.4,len(path)),8)
    m.tube('Streamer fine gold edging','polished_gold',path+[0,0,1.7],.3,4)
    m.sphere('Tassel gold knot','polished_gold',path[-1],[2.5,2.5,2.5],5,10)
    for j in range(7):
        a=TAU*j/7
        m.tube('Fine dangling gold tassel','polished_gold',
               [path[-1],path[-1]+[math.cos(a)*2,-5,math.sin(a)*2],path[-1]+[math.cos(a)*2.6,-8,math.sin(a)*2.6]],.35,4)
    m.transform(offset=[-5,-7,0])
    m.markers=dict(brim_opening_author=[-5,-21,0],presentation_only=True)
    m.notes=['Exact supplied Minish Cap icon: crimson bent cone, brocade gold brim with red gems, tip orb and gold tassel.',
             'GI only; Minish shrink/grow ability and player appearance are unchanged.',
             'Caller .5, effective author scale .6; hollow brim and modeled cloth folds.']
    return m


def _cape_shell(m, name, mat, points, inward=False):
    """A cloth sheet with explicitly oriented smooth normals."""
    grid=np.asarray(points,float);rows,cols=grid.shape[:2]
    du=np.gradient(grid,axis=0);dv=np.gradient(grid,axis=1)
    normals=unit(np.cross(du,dv))
    radial=np.stack([grid[:,:,0],np.zeros((rows,cols)),grid[:,:,2]],axis=-1)
    if np.mean(np.sum(normals*radial,axis=-1))<0:normals=-normals
    if inward:normals=-normals
    faces=[]
    for i in range(rows-1):
        for j in range(cols-1):
            a=i*cols+j;b=a+cols;faces += [[a,b,a+1],[a+1,b,b+1]]
    uv=np.stack(np.meshgrid(np.linspace(0,1,cols),np.linspace(0,1,rows)),axis=-1)
    m.add(name,mat,grid.reshape(-1,3),normals.reshape(-1,3),uv.reshape(-1,2),faces)


def rocs_cape():
    m=_model('rocs_cape', "Roc's Cape", .6, .52)
    m.material('ivory_cloth',[.91,.93,.90],tex=_cloth([.98,.985,.965]),rough=.8)
    m.material('cool_lining',[.38,.43,.47],tex=_cloth([.44,.48,.50]),rough=.88)
    m.material('white_seam',[.97,.98,.94],tex=_cloth([1,1,.97]),rough=.7)
    m.material('blue_feather',[.055,.19,.43],'feather',0,.7)
    m.material('cyan_feather',[.12,.57,.72],'feather',0,.7)
    m.material('deep_blue',[.018,.075,.20],'feather',0,.78)
    m.material('clasp_gold',[.90,.58,.12],'metal',.8,.3)
    m.material('clasp_inset',[.16,.12,.06],None,.5,.5)

    # The icon is an open white shoulder cape: a real neck opening, tall collar,
    # two front tails and a shorter back, with pale gray fabric inside.
    angles=np.linspace(math.radians(24),TAU-math.radians(24),65)
    rows=[]
    for t in np.linspace(0,1,15):
        rx=17+(34-17)*t
        rz=12+(24-12)*t
        row=[]
        for a in angles:
            fold=math.sin(a*6+.25)*2.7*math.sin(math.pi*t*.84)
            hem=-26-13*math.cos(a)
            y=21*(1-t)+hem*t+.7*math.sin(a*6)*math.sin(math.pi*t)
            # Back drapes outward while both front tails curl gently inward.
            drape_angle=a+.12*math.sin(math.pi*t)*math.sin(a*2)
            row.append([(rx+fold)*math.sin(drape_angle),y,
                        (rz+fold)*math.cos(drape_angle)+3*math.sin(math.pi*t)])
        rows.append(row)
    outer=np.array(rows)
    radial=unit(outer*np.array([1,0,1]))
    inner=outer-radial*.8
    _cape_shell(m,'White draped cape','ivory_cloth',outer)
    _cape_shell(m,'Visible gray inner cape','cool_lining',inner,True)
    for j in [0,-1]:
        m.tube('White rolled front opening seam','white_seam',outer[:,j],.65,6)
    m.tube('White hem rolled edge','white_seam',outer[-1],.62,6)

    # A raised collar, open through the middle and split at the clasp.
    collar=[]
    for t in np.linspace(0,1,5):
        collar.append([[math.sin(a)*(17+1.9*t),21+15*t+1.3*math.cos(a),
                        math.cos(a)*(12+1.5*t)] for a in angles])
    collar=np.array(collar);normal=unit(collar*np.array([1,0,1]))
    _cape_shell(m,'Upright white open collar','ivory_cloth',collar)
    _cape_shell(m,'Gray inside collar','cool_lining',collar-normal*.9,True)
    m.tube('White rolled collar rim','white_seam',collar[-1],.8,7)
    for j in [0,-1]:m.tube('Collar front edge','white_seam',collar[:,j],.7,6)

    # A narrow blue stitched back edge and two broad groups of flight feathers
    # reproduce the icon's distinct front tips instead of a fringe of pendants.
    hem_edge=outer[-1,7:-7]
    hem_near=outer[-2,7:-7]
    hem_normal=unit(hem_edge*np.array([1,0,1]))
    hem_band=[hem_edge*.68+hem_near*.32+hem_normal*.18,
              hem_edge+unit(hem_edge-hem_near)*.6+hem_normal*.18]
    _cape_shell(m,'Coherent dark blue back hem','deep_blue',hem_band)
    tail_angles=[28,42,56,304,318,332]
    for j,deg in enumerate(tail_angles):
        a=math.radians(deg)
        hem=-26-13*math.cos(a)
        tip_len=16 if j in [1,4] else 12.5
        pos=np.array([np.interp(a,angles,outer[-1,:,k]) for k in range(3)])
        pos+=unit(pos*np.array([1,0,1]))*.85
        start=len(m.parts)
        m.polygon('Pointed blue flight feather %02d'%j,'blue_feather',
                  [(-5.4,3.5),(-1,5.3),(4.6,3.2),(6,-1),(4.3,-6),
                   (2.4,-tip_len*.72),(.5,-tip_len),(-2,-tip_len*.70),(-5.4,-5),(-6,-.5)],
                  depth=.85,z=0,bevel=.35)
        m.polygon('Cyan feather shoulder %02d'%j,'cyan_feather',
                  [(-5.4,3.6),(-1,5.4),(4.6,3.3),(6,-1),(4.8,-4.5),(1.3,-2.4),
                   (-2.4,-4.0),(-5.5,-3.2),(-6,-.4)],
                  depth=1.02,z=.05,bevel=.2)
        m.tube('Feather blue quill','deep_blue',[[.1,-2,.57],[1,-tip_len*.55,.55],[.5,-tip_len+.65,.46]],.19,4)
        m.transform(rotation('y',math.degrees(a)),offset=pos,start=start)

    # One modest gold clasp across the throat, matching the supplied icon.
    m.tube('Short gold collar fastener','clasp_gold',[[-6.9,21.3,11.0],[6.9,21.3,11.0]],.8,7)
    m.sphere('Round gold neck clasp','clasp_gold',[-6.8,20.8,11.8],[2.9,3.3,1.1],6,12)
    m.sphere('Clasp dark inset','clasp_inset',[-6.8,20.8,12.6],[1.35,1.65,.55],5,10)
    m.transform(offset=[0,4,0])
    m.markers=dict(collar_author=[0,39,0],presentation_only=True)
    m.notes=["Exact supplied Roc's Cape icon: white open collar, gray lining, single gold clasp and blue/cyan pointed feather tips.",
             'Two front tails and shorter back are modeled cloth surfaces with explicit lining and edge thickness.',
             'GI only; existing Roc jump/glide behavior is preserved. Caller .6, effective scale .52.']
    return m


BUILDERS={'spinner':spinner,'cane_of_somaria':cane_of_somaria,
          'minish_cap':minish_cap,'rocs_cape':rocs_cape}
