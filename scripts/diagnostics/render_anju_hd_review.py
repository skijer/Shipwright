"""Render review files by reading the final HD archive, including its blink heads."""
from pathlib import Path
import argparse
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from render_anju_preview import Anju, render

PREFIX = 'objects/object_anju_hd/v1/'
HEADS = ['gAnju1HeadDL', 'gAnju1BlinkHalfHeadDL', 'gAnju1BlinkClosedHeadDL']
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('model', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    title = ImageFont.truetype(FONT, 25)
    label = ImageFont.truetype(FONT, 18)
    model = Anju(args.native, False, args.model)
    frames, bounds = [], []
    for frame in range(len(model.clips[model.animation])):
        state = {30: 1, 31: 2, 32: 2, 33: 1}.get(frame, 0)
        model.dlists[8] = PREFIX + HEADS[state]
        parts = model.pose(model.animation, frame)
        points = np.concatenate([p['pos'] for p in parts])
        bounds.append([points.min(0).tolist(), points.max(0).tolist()])
        sheet = Image.new('RGB', (1120, 830), (22, 29, 42))
        draw = ImageDraw.Draw(sheet)
        draw.text((24, 16), 'ANJU  |  HD auburn & umbrella  |  0x7E0D', font=title, fill='#eef2fa')
        draw.text((24, 52), 'Archive readback • Native seated loop at 20 fps • Offline lighting', font=label, fill='#aebcd0')
        for index, (yaw, text) in enumerate([(12, 'Front'), (105, 'Side / heels')]):
            panel = render(parts, (560, 640), yaw=yaw, scale=.080, center_y=3000)
            p = ImageDraw.Draw(panel)
            origin = 320 + int(3000*.080)
            p.line((22, origin, 538, origin), fill='#68778a')
            p.text((24, origin+8), 'Placed Y — remains fixed', font=label, fill='#aebcd0')
            sheet.paste(panel, (560*index, 112))
            draw.text((560*index+24, 87), text, font=label, fill='#e6c479')
        draw.text((24, 765), 'Original umbrella proportions and hand attachment; sharper canopy pattern.', font=label, fill='#d2dcea')
        draw.text((24, 795), 'Align her seat with your furniture. This preview is not an in-game capture.', font=label, fill='#aebcd0')
        frames.append(sheet)
        if frame == 0:
            sheet.save(args.output/'Anju_HD_Umbrella_Preview.png')
        if frame % 10 == 0:
            print('Rendered native frame', frame, flush=True)
    frames[0].save(args.output/'Anju_HD_Umbrella_Preview.gif', save_all=True,
                   append_images=frames[1:], duration=50, loop=0, disposal=2)
    (args.output/'animation_bounds.json').write_text(json.dumps(bounds, indent=2))

    # Isolate the actual exported head under one neutral camera transform. No
    # source-model edits or substituted textures enter the readback preview.
    head = Anju(args.native, True, args.model)
    head.dlists = [''] * 20
    head.umbrella = ''
    matrices = head.world(head.clips[head.animation][0])
    basis = np.eye(4)
    basis[:3, :3] = np.array([[0, 0, -1], [1, 0, 0], [0, 1, 0]])
    matrices = basis @ np.linalg.inv(matrices[8]) @ matrices
    head.world = lambda frame: matrices
    blink = []
    for index, name in enumerate(HEADS):
        head.dlists[8] = PREFIX + name
        panel = render(head.pose(head.animation), (720, 710), scale=.64, center_y=620, yaw=0)
        p = ImageDraw.Draw(panel)
        p.text((20, 16), ['Open', 'Half', 'Closed'][index], font=title, fill='#eef2fa')
        p.text((20, 678), 'Actual exported head • Offline preview', font=label, fill='#aebcd0')
        panel.save(args.output/f'Anju_Blink_{index}.png')
        blink.append(panel)
    blink[0].save(args.output/'Anju_HD_Blink_Preview.gif', save_all=True,
                  append_images=[blink[1], blink[2], blink[1]], duration=[1500, 50, 100, 50], loop=0, disposal=2)
    contact = Image.new('RGB', (1080, 390), (22, 29, 42))
    for index, panel in enumerate(blink):
        contact.paste(panel.resize((360, 355), Image.Resampling.LANCZOS), (360*index, 0))
    ImageDraw.Draw(contact).text((18, 365), 'Blink frames: open → half → fully closed → half → open', font=label, fill='#d2dcea')
    contact.save(args.output/'Anju_HD_Blink_Frames.png')
    standing = Anju(args.native, True, args.model)
    heels = render(standing.pose(standing.animation), (800, 650), yaw=105, scale=.38, center_y=750)
    heels.save(args.output/'Anju_HD_Heels.png')
    print('Review files:', args.output, flush=True)


if __name__ == '__main__':
    main()
