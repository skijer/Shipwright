"""Render the exact lantern GLB and exported production effects, without bloom."""
import argparse
from pathlib import Path
import subprocess
from PIL import ImageDraw
import render as r


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('effects', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--frames', type=int, default=180)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fx, orbs = r.effects(args.effects), r.orbs(args.effects)
    opaque, glass = r.model('lantern')
    output = args.output / 'NEI_Lantern_POC1_variants.mp4'
    proc = subprocess.Popen([
        'ffmpeg', '-loglevel', 'error', '-y', '-f', 'rawvideo', '-vcodec', 'rawvideo',
        '-pix_fmt', 'rgb24', '-s', f'{r.W}x{r.H}', '-r', '20', '-i', '-', '-an',
        '-c:v', 'libx264', '-preset', 'fast', '-crf', '18', '-pix_fmt', 'yuv420p',
        '-movflags', '+faststart', str(output)], stdin=subprocess.PIPE)
    names = ['Regular fire', 'Blue fire', 'Poe fire', 'Green fire']
    notes = ['Rising tongues and embers', 'Cold core and falling frost',
             'Orbiting wisps and fading trails', 'Gentle growth curls and seed motes']
    for frame in range(args.frames):
        r.DepthMask(1)
        r.Clear(0x4000 | 0x0100)
        for i in range(4):
            r.pose((i % 2) * 600, 60 + (1 - i // 2) * 435, 600, 365, -22, 34)
            r.DepthMask(1)
            r.CallList(opaque)
            r.DepthMask(0)
            r.Push()
            # GLB model() already includes the GI scale; effect samples are in
            # author units, exactly as the held renderer consumes them.
            r.Scale(.5, .5, .5)
            r.draworb(orbs[frame][i])
            r.drawfx(fx[frame][i])
            r.Pop()
            r.CallList(glass)
        im = r.pixels()
        draw = ImageDraw.Draw(im)
        draw.rectangle((0, 0, r.W, 72), fill='#111923')
        draw.text((30, 14), 'Lantern — four contained fire variants', font=r.font(29), fill='#f0f5ff')
        draw.text((30, 49), 'Actual shared model + production effect geometry and native texels',
                  font=r.font(15), fill='#aabacc')
        for i in range(4):
            x, y = (i % 2) * 600 + 30, 91 + (i // 2) * 435
            draw.text((x, y), names[i], font=r.font(25), fill='#eef3fd')
            draw.text((x, y + 33), notes[i], font=r.font(16), fill='#aabacc')
        draw.text((30, 972), 'Offline preview • in-game grip, lighting, camera and interpolation still need review.',
                  font=r.font(16), fill='#92a4ba')
        if frame == 36 or args.frames == 1:
            im.save(args.output / 'NEI_Lantern_POC1_variants.png')
        proc.stdin.write(im.tobytes())
    proc.stdin.close()
    if proc.wait() != 0:
        raise RuntimeError('ffmpeg failed')
    print(output)


if __name__ == '__main__':
    main()
