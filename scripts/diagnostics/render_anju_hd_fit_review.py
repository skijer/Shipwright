"""Preview the revised HD archive with native seated and standing umbrella loops."""
from pathlib import Path
import argparse
import copy
import io
import json
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from render_anju_preview import Anju, render

PREFIX = 'objects/object_anju_hd/v1/'
HEADS = ['gAnju1HeadDL', 'gAnju1BlinkHalfHeadDL', 'gAnju1BlinkClosedHeadDL']
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
TITLE = ImageFont.truetype(FONT, 25)
LABEL = ImageFont.truetype(FONT, 18)
BG = (22, 29, 42)


def select_standing_hand(model):
    candidate = PREFIX + 'gAnju1RelaxedLeftHandDL'
    if candidate in model.resources:
        model.dlists[4] = candidate
    for limb in (2, 3, 6, 7):
        candidate = model.dlists[limb].removesuffix('DL') + 'StandingDL'
        if candidate in model.resources:
            model.dlists[limb] = candidate
    candidate = PREFIX + 'gAnju2UmbrellaStandingDL'
    if candidate in model.resources:
        model.umbrella = candidate


def save_png(image, path):
    payload = io.BytesIO()
    image.save(payload, format='PNG')
    path.write_bytes(payload.getvalue())
    Image.open(path).verify()


def render_loop(native, archive, output, standing, revision='R2', palette=None):
    model = Anju(native, standing, archive)
    if standing:
        select_standing_hand(model)
    name = 'Standing' if standing else 'Seated'
    stem = f'Anju_HD_{palette + "_" if palette else ""}{name}_Umbrella_{revision}'
    # Decode the exported graph at every native frame before fitting a fixed
    # camera. The umbrella tip and actor origin must both remain in view.
    poses, bounds = [], []
    for frame in range(len(model.clips[model.animation])):
        start = 20 if standing else 30
        state = {start: 1, start + 1: 2, start + 2: 2, start + 3: 1}.get(frame, 0)
        model.dlists[8] = PREFIX + HEADS[state]
        parts = model.pose(model.animation, frame)
        points = np.concatenate([p['pos'] for p in parts])
        bounds.append([points.min(0).tolist(), points.max(0).tolist()])
        poses.append(parts)
    bounds = np.array(bounds)
    low, high = bounds[:, 0].min(0), bounds[:, 1].max(0)
    low[1] = min(0, low[1])
    center_y = (low[1] + high[1]) / 2
    corners = np.array(np.meshgrid(*zip(low, high))).T.reshape(-1, 3)
    yaw_values = [12, 105]
    width_limit = 0
    for yaw in yaw_values:
        a = np.deg2rad(yaw)
        width_limit = max(width_limit, np.max(np.abs(corners[:, 0] * np.cos(a) - corners[:, 2] * np.sin(a))))
    scale = min(620 / (high[1] - low[1]), 250 / width_limit)
    frames = []
    for frame, parts in enumerate(poses):
        sheet = Image.new('RGB', (1120, 920), BG)
        draw = ImageDraw.Draw(sheet)
        pose_label = 'Standing umbrella idle' if standing else 'Seated umbrella crying'
        param_label = 'Candidate pose' if standing else '0x7E0D'
        title = f'ANJU {revision}' + (f' {palette.upper()}' if palette else '')
        draw.text((24, 16), f'{title}  |  {pose_label}  |  {param_label}', font=TITLE, fill='#eef2fa')
        draw.text((24, 52), 'Actual archive readback • Native motion at 20 fps • Offline preview', font=LABEL, fill='#aebcd0')
        for index, (yaw, text) in enumerate(zip(yaw_values, ['Front', 'Side'])):
            panel = render(parts, (560, 730), yaw=yaw, scale=scale, center_y=center_y)
            panel_draw = ImageDraw.Draw(panel)
            origin = round(365 + center_y * scale)
            panel_draw.line((22, origin, 538, origin), fill='#68778a')
            panel_draw.text((24, origin + 8), 'Placed Y — fixed', font=LABEL, fill='#aebcd0')
            sheet.paste(panel, (560 * index, 112))
            draw.text((560 * index + 24, 87), text, font=LABEL, fill='#e6c479')
        changes = ('Clean shoe fronts and vest hem • Relaxed standing hand • Approved hair and fabric' if revision == 'R3'
                   else 'Wrapped umbrella grip • Fitted skirt • Approved auburn hair and fabric')
        if revision == 'R4':
            changes = 'Natural wrist and clear grip • Joined elbow skin • Gentler left ankle taper'
        elif revision == 'R5':
            changes = 'Folded thumb • Close umbrella grip • Original arm and sleeve fit restored'
        if palette == 'Goth':
            changes = 'Black hair and brows • Charcoal clothes and heels • Silver trim • Exact R5 geometry'
        draw.text((24, 852), changes, font=LABEL, fill='#d2dcea')
        footer = ('Standing animation preview; no catalogue parameter assigned yet.' if standing else
                  'Align her seat with your furniture. Placement anchors and native animation are unchanged.')
        draw.text((24, 882), footer, font=LABEL, fill='#aebcd0')
        if frame == 0:
            save_png(sheet, output / (stem + '.png'))
        frames.append(sheet)
        if frame % 10 == 0:
            print(name, 'frame', frame, flush=True)
    frames[0].save(output / (stem + '.gif'), save_all=True, append_images=frames[1:],
                   duration=50, loop=0, disposal=2)
    (output / (stem + '_bounds.json')).write_text(json.dumps(bounds.tolist(), indent=2))
    print(name, 'complete:', len(frames), 'frames', flush=True)


def final_fit_details(native, archive, output):
    from anju_hd_finish import grip_basis

    sheet = Image.new('RGB', (1120, 1300), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 16), 'ANJU R4  |  Grip and wrist', font=TITLE, fill='#eef2fa')
    draw.text((24, 52), 'Actual archive geometry • Both native umbrella poses • Offline lighting', font=LABEL, fill='#aebcd0')
    for row, standing in enumerate((True, False)):
        model = Anju(native, standing, archive)
        if standing:
            select_standing_hand(model)
        world = model.world(model.clips[model.animation][0])
        center = world[7, :3, :3] @ (grip_basis(world) @ [230, 0, 50]) + world[7, :3, 3]
        parts = copy.deepcopy(model.pose(model.animation))
        for part in parts:
            part['pos'] -= center
        label = 'Standing candidate' if standing else 'Seated crying — 0x7E0D'
        draw.text((24, 88 + row * 590), label, font=LABEL, fill='#e6c479')
        for column, yaw in enumerate((12, 105)):
            panel = render(parts, (560, 540), yaw=yaw, scale=.5, center_y=0)
            sheet.paste(panel, (column * 560, 118 + row * 590))
    save_png(sheet, output / 'Anju_HD_Grip_Wrist_R4.png')

    sheet = Image.new('RGB', (1120, 1300), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 16), 'ANJU R4  |  Left elbow and ankle', font=TITLE, fill='#eef2fa')
    draw.text((24, 52), 'Continuous elbow skin • Ankle taper above the unchanged sole', font=LABEL, fill='#aebcd0')
    for column, standing in enumerate((True, False)):
        model = Anju(native, standing, archive)
        if standing:
            select_standing_hand(model)
        world = model.world(model.clips[model.animation][0])
        parts = copy.deepcopy(model.pose(model.animation))
        for part in parts:
            part['pos'] -= world[3, :3, 3]
        sheet.paste(render(parts, (560, 530), yaw=105, scale=.65, center_y=0), (560 * column, 120))
        draw.text((560 * column + 24, 88), 'Standing elbow' if standing else 'Seated elbow', font=LABEL, fill='#e6c479')
    model = Anju(native, True, archive)
    select_standing_hand(model)
    world = model.world(model.clips[model.animation][0])
    center = world[15, :3, :3] @ [250, 520, 0] + world[15, :3, 3]
    parts = copy.deepcopy(model.pose(model.animation))
    for part in parts:
        part['pos'] -= center
    for column, yaw in enumerate((0, 105)):
        sheet.paste(render(parts, (560, 530), yaw=yaw, scale=.40, center_y=0), (560 * column, 715))
        draw.text((560 * column + 24, 681), 'Ankle — front' if column == 0 else 'Ankle — side', font=LABEL, fill='#e6c479')
    save_png(sheet, output / 'Anju_HD_Elbow_Ankle_R4.png')


def contact_fit_details(native, archive, output):
    from anju_hd_finish import grip_basis

    sheet = Image.new('RGB', (1440, 1240), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 16), 'ANJU R5  |  Folded thumb and umbrella contact', font=TITLE, fill='#eef2fa')
    draw.text((24, 52), 'Actual archive geometry • Front and both sides • Offline preview', font=LABEL, fill='#aebcd0')
    sleeve = Image.new('RGB', (1120, 1230), BG)
    sleeve_draw = ImageDraw.Draw(sleeve)
    sleeve_draw.text((24, 16), 'ANJU R5  |  Original arm and sleeve fit restored', font=TITLE, fill='#eef2fa')
    sleeve_draw.text((24, 52), 'R3 left arm geometry • R4 elbow rebuild removed', font=LABEL, fill='#aebcd0')
    for row, standing in enumerate((True, False)):
        model = Anju(native, standing, archive)
        if standing:
            select_standing_hand(model)
        world = model.world(model.clips[model.animation][0])
        original = model.pose(model.animation)
        center = world[7, :3, :3] @ (grip_basis(world) @ [230, 0, 50]) + world[7, :3, 3]
        parts = copy.deepcopy(original)
        for part in parts:
            part['pos'] -= center
        label = 'Standing candidate' if standing else 'Seated crying — 0x7E0D'
        draw.text((24, 88 + row * 560), label, font=LABEL, fill='#e6c479')
        for column, yaw in enumerate((12, 105, -105)):
            sheet.paste(render(parts, (480, 500), yaw=yaw, scale=.45, center_y=0), (480 * column, 118 + row * 560))
        parts = copy.deepcopy(original)
        # Include the sleeve and forearm together, not only the elbow crease.
        center = world[3, :3, 3] + [0, 120, 0]
        for part in parts:
            part['pos'] -= center
        sleeve_draw.text((24, 88 + row * 560), label, font=LABEL, fill='#e6c479')
        for column, yaw in enumerate((12, 105)):
            sleeve.paste(render(parts, (560, 500), yaw=yaw, scale=.52, center_y=0), (560 * column, 118 + row * 560))
    save_png(sheet, output / 'Anju_HD_Grip_Contact_R5.png')
    save_png(sleeve, output / 'Anju_HD_Arm_Sleeve_R5.png')


def grip_detail(native, archive, output, revision='R2'):
    model = Anju(native, False, archive)
    matrix = model.world(model.clips[model.animation][0])[7]
    center = matrix[:3, :3] @ [260, 0, 100] + matrix[:3, 3]
    parts = copy.deepcopy(model.pose(model.animation))
    for part in parts:
        part['pos'] -= center
    sheet = Image.new('RGB', (1120, 690), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 18), f'ANJU {revision}  |  Umbrella grip detail', font=TITLE, fill='#eef2fa')
    draw.text((24, 54), 'Actual exported hand, nails and unchanged native shaft', font=LABEL, fill='#aebcd0')
    for index, yaw in enumerate([12, -105]):
        panel = render(parts, (560, 550), yaw=yaw, scale=.52, center_y=0)
        sheet.paste(panel, (560 * index, 92))
    draw.text((24, 656), 'Offline lighting; the opposite hand is preserved.', font=LABEL, fill='#aebcd0')
    sheet.save(output / f'Anju_HD_Umbrella_Grip_{revision}.png')


def fit_details(native, archive, output, revision):
    model = Anju(native, True, archive)
    select_standing_hand(model)
    parts = model.pose(model.animation)
    sheet = Image.new('RGB', (1200, 790), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 16), f'ANJU {revision}  |  Shoes and vest close-up', font=TITLE, fill='#eef2fa')
    draw.text((24, 52), 'Actual archive geometry • Standing front view • Offline lighting', font=LABEL, fill='#aebcd0')
    views = [('Shoe fronts', [40, 440, 100], .35), ('Vest and waistband', [30, 4340, 0], .39)]
    for index, (label, center, scale) in enumerate(views):
        detail = copy.deepcopy(parts)
        for part in detail:
            part['pos'] -= center
        panel = render(detail, (600, 610), yaw=0, scale=scale, center_y=0)
        sheet.paste(panel, (600 * index, 118))
        draw.text((600 * index + 24, 90), label, font=LABEL, fill='#e6c479')
    draw.text((24, 748), 'Continuous soles • Tucked vest hem • Original textures and ankle placement', font=LABEL, fill='#d2dcea')
    sheet.save(output / f'Anju_HD_Shoes_Vest_{revision}.png')

    matrix = model.world(model.clips[model.animation][0])[4]
    center = matrix[:3, :3] @ [440, -10, -30] + matrix[:3, 3]
    for part in parts:
        part['pos'] -= center
    sheet = Image.new('RGB', (1120, 690), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 18), f'ANJU {revision}  |  Relaxed standing hand candidate', font=TITLE, fill='#eef2fa')
    draw.text((24, 54), 'Softer fingers; separate from her original crying hand', font=LABEL, fill='#aebcd0')
    for index, yaw in enumerate([12, 100]):
        sheet.paste(render(parts, (560, 550), yaw=yaw, scale=.55, center_y=0), (560 * index, 92))
    draw.text((24, 656), 'Actual alternate hand resource • Standing remains a preview candidate', font=LABEL, fill='#aebcd0')
    sheet.save(output / f'Anju_HD_Relaxed_Hand_{revision}.png')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('model', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--details-only', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.model) as archive:
        metadata = json.loads(archive.read('Anju_HD_README.json'))
    revision = metadata.get('revision', 'R1')
    palette = metadata.get('palette')
    if revision == 'R3':
        fit_details(args.native, args.model, args.output, revision)
    elif revision == 'R4':
        final_fit_details(args.native, args.model, args.output)
    elif revision == 'R5' and not palette:
        contact_fit_details(args.native, args.model, args.output)
    if args.details_only:
        return
    if revision not in ('R4', 'R5'):
        grip_detail(args.native, args.model, args.output, revision)
    for standing in [True, False]:
        render_loop(args.native, args.model, args.output, standing, revision, palette)
    print('Review files:', args.output, flush=True)


if __name__ == '__main__':
    main()
