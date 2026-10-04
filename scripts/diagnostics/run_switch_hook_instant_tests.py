"""Exercise production Switch Hook launch/selection/swap code with real game structs.

Engine audio services are fixtures; actor selection, charge spending, collider
reanchoring, launch branching and both position transitions are production functions.
Set LUS_INCLUDE for a configured engine checkout when this submodule is absent.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def function(source, name):
    match = re.search(r'^(?:static )?[^\n;{}]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    assert match, name
    start = match.start()
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'

def main():
    src = (ROOT / 'soh/src/overlays/actors/ovl_Arms_Hook/z_arms_hook.c').read_text()
    item = (ROOT / 'soh/mods/items/logic/item_switchhook.c').read_text()
    selection = (ROOT / 'soh/mods/items/helpers/target_select_helper.c').read_text()
    generated = '#include "switch_hook_instant_fixture.h"\n'
    generated += src[src.index('#define ARMSHOOK_SWAP_HOLD_FRAMES'):src.index('s32 SwitchHook_PlayerNoClip')]
    generated += 'static Actor* sSwitchSelection;\n'
    generated += item[item.index('#define SWITCHHOOK_MAX_CHARGES'):item.index('// Per-frame tick')]
    generated += 'static u8 sShAimManual;\n'
    generated += item[item.index('void SwitchHook_ShiftCollider('):item.index('/** Drop every tracked actor')]
    generated += function(item, 'SwitchHook_ClearSwapColliders')
    generated += selection[selection.index('#define TARGETSEL_LIST_HEAD'):]
    for name in ['SwitchHook_ConsumeCharge', 'SwitchHook_GetCharges', 'SwitchHook_IsAimingManual', 'SwitchHook_OnFired']:
        generated += function(item, name)
    for name in ['ArmsHook_IsSwappable', 'ArmsHook_SelectSwapCandidate', 'ArmsHook_FindSwappable',
                 'ArmsHook_IsLiveSwapTarget', 'ArmsHook_SelectManualSwapCandidate',
                 'ArmsHook_SetupAction', 'ArmsHook_AttachToPlayer', 'ArmsHook_StartSwap',
                 'ArmsHook_ReleaseAfterSwap', 'ArmsHook_Wait', 'ArmsHook_SwitchSwap', 'ArmsHook_Update']:
        if re.search(r'\b' + name + r'\([^;{}]*\)\s*\{', src):
            generated += function(src, name)
    generated += (ROOT / 'soh/tests/switch_hook_instant_test.c').read_text()
    flags = ['-std=gnu2x', '-O1', '-g', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=0', '-Werror=implicit-function-declaration']
    for path in ['soh/include', 'soh/src', 'soh/assets', 'soh', 'soh/tests']:
        flags += ['-I' + str(ROOT / path)]
    flags += ['-I' + os.environ.get('LUS_INCLUDE', str(ROOT / 'libultraship/include'))]
    for path in ['CMake/soh-cvars.cmake', 'CMake/lus-cvars.cmake']:
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags += [f'-D{key}="{value}"']
    with tempfile.TemporaryDirectory(prefix='switch-hook-instant-') as temp:
        unit = Path(temp) / 'test.c'
        unit.write_text(generated)
        binary = Path(temp) / 'test'
        subprocess.run([os.environ.get('CC', 'cc'), *flags, str(unit), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
