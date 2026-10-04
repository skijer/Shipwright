"""Geometry/clearance comparison, using EnGirlA's shelf plane at local Y=-24."""
from pathlib import Path
from PIL import ImageDraw
import render as r
import sys

def shelf():
 r.Disable(0x0DE1);r.Disable(0x0B44)
 # A schematic counter, not a replacement shop scene or a gameplay screenshot.
 r.Begin(0x0007)
 for color,points in [((.25,.16,.095),[(-58,-24,-32),(-58,-24,32),(58,-24,32),(58,-24,-32)]),
                      ((.15,.09,.055),[(-58,-24,32),(-58,-45,32),(58,-45,32),(58,-24,32)])]:
  r.Color(*color,1)
  for p in points:r.Vertex(*p)
 r.End()

items=[('ball_and_chain','Ball and Chain',.62,9),('shovel','Shovel',.82,13),('fire_rod','Rods',.85,14)]
models=[r.model(x[0]) for x in items]
r.DepthMask(1);r.Clear(0x4000|0x0100)
for row in range(2):
 for i,(_,_,scale,lift) in enumerate(items):
  r.pose(i*400,80+(1-row)*445,400,350,-52,65);shelf();r.Push()
  if row:r.Translate(0,lift,0);r.Scale(scale,scale,scale)
  r.CallList(models[i][0]);r.Pop()
im=r.pixels();d=ImageDraw.Draw(im)
d.rectangle((0,0,1200,72),fill='#111923');d.text((28,17),'NEI GI • shop clearance — POC4',font=r.font(28),fill='#eef5ff')
d.text((28,53),'Actual exported vertices and proposed shop transforms · schematic counter · fixed comparison scale',font=r.font(15),fill='#aabacc')
for row in range(2):
 for i,(_,name,scale,lift) in enumerate(items):
  y=82+row*445;x=i*400
  d.text((x+20,y),name+(' · before' if not row else ' · revised'),font=r.font(23),fill='#eef5ff')
  d.text((x+20,y+32),'Original shop size' if not row else f'{round(scale*100)}% size + raised shelf pose',font=r.font(16),fill='#aabacc')
d.text((28,967),'Shop display only. World pickups and overhead model sizes are preserved.',font=r.font(20),fill='#aabacc')
out=Path(sys.argv[1]);im.save(out);print(out)
