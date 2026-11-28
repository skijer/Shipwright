"""Checks custom music sequences in .otr and .o2r packs against the game's sequence player and OoT's soundfonts.

Unsafe findings make the player read or write outside the sequence data or its own arrays; silent ones drop notes.
"""
import argparse
import struct
import sys
import zipfile
from collections import Counter, deque
from dataclasses import dataclass, replace
from pathlib import Path

RESOURCE_HEADER_SIZE = 0x40
CUSTOM_MUSIC_PREFIX = b"custom/music/"
FONT_PREFIX = "audio/fonts/"
STREAMED_FONT_COUNT = 0xFFFFFFFF
MAX_SEQUENCE_FONTS = 16
SCRIPT_STACK_DEPTH = 4
IO_PORT_COUNT = 8
CHANNEL_COUNT = 16
MAX_STEPS = 200_000

# D_80130520 in audio_seqplayer.c, from opcode 0xB0: the low 2 bits count arguments, each set high bit marks an s16.
ARGUMENT_TABLE = bytes([
    0x81, 0x00, 0x81, 0x01, 0x00, 0x00, 0x00, 0x81, 0x01, 0x01, 0x01, 0x42, 0x81, 0xC2, 0x00, 0x00,
    0x00, 0x01, 0x81, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x81, 0x01, 0x01, 0x81, 0x81,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x81, 0x01, 0x01, 0x01, 0x81, 0x01,
    0x01, 0x03, 0x03, 0x01, 0x00, 0x01, 0x01, 0x81, 0x03, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0x82,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x81, 0x00, 0x00, 0x01, 0x81, 0x81, 0x81, 0x81, 0x00, 0x00, 0x00,
])
SEQUENCE_ARGUMENTS = {
    0xF1: "b", 0xDF: "b", 0xDE: "b", 0xDD: "b", 0xDC: "b", 0xDA: "bh", 0xDB: "b", 0xD9: "b", 0xD7: "h",
    0xD6: "h", 0xD5: "b", 0xD3: "b", 0xD1: "h", 0xD2: "h", 0xD0: "b", 0xCE: "b", 0xCD: "h", 0xCC: "b",
    0xC9: "b", 0xC8: "b", 0xC7: "bh", 0xC5: "h", 0xEF: "hb", 0xC4: "bb",
}
LAYER_BYTE_ARGUMENTS = {0xC1, 0xCA, 0xC9, 0xC2, 0xCF, 0xCD, 0xCE}

UNSAFE_KINDS = {
    "truncated", "pointer_out_of_range", "write_out_of_range", "font_index_out_of_range", "too_many_fonts",
    "io_write_out_of_range", "io_read_out_of_range", "channel_index_out_of_range", "stack_overflow",
    "stack_underflow", "malformed_resource",
}
SILENT_KINDS = {"instrument_missing", "drum_missing", "sfx_missing", "unknown_font", "analysis_limit"}


@dataclass(frozen=True)
class Font:
    instruments: tuple
    drums: tuple
    sound_effects: tuple


@dataclass(frozen=True)
class Finding:
    kind: str
    offset: int
    detail: str


@dataclass(frozen=True)
class SequenceState:
    pc: int
    stack: tuple = ()


@dataclass(frozen=True)
class ChannelState:
    pc: int
    font: object
    index: int = 0
    stack: tuple = ()
    large_notes: bool = False
    transposition: int = 0
    instrument: str = "none"
    started_layers: tuple = ()


@dataclass(frozen=True)
class LayerState:
    pc: int
    channel: ChannelState
    stack: tuple = ()
    transposition: int = 0
    instrument: str = "channel"


class Truncated(Exception):
    pass


class ScriptCursor:
    def __init__(self, data, pc):
        self.data = data
        self.pc = pc

    def read_u8(self):
        if self.pc >= len(self.data):
            raise Truncated()
        value = self.data[self.pc]
        self.pc += 1
        return value

    def read_s16(self):
        value = (self.read_u8() << 8) | self.read_u8()
        return value - 0x10000 if value & 0x8000 else value

    def read_u16(self):
        return self.read_s16() & 0xFFFF

    def read_compressed(self):
        first = self.read_u8()
        if not first & 0x80:
            return first
        return ((first << 8) & 0x7F00) | self.read_u8()

    def read_table_arguments(self, command):
        entry = ARGUMENT_TABLE[command - 0xB0]
        arguments = []
        for _ in range(entry & 3):
            arguments.append(self.read_s16() & 0xFFFF if entry & 0x80 else self.read_u8())
            entry = (entry << 1) & 0xFF
        return arguments


def to_s8(value):
    return value - 0x100 if value & 0x80 else value


def instrument_mode(value):
    if value == 0x7F:
        return "drum"
    if value == 0x7E:
        return "sfx"
    if value >= 0x80:
        return "wave"
    return "instrument"


class SequenceAnalyzer:
    def __init__(self, data, fonts, font_table):
        self.data = data
        self.fonts = list(fonts)
        self.font_table = font_table
        self.findings = set()
        self.pending = deque()
        self.visited = set()
        self.enabled_channels = set()
        self.reenables = set()
        self.channel_exits = {}

    # Re-enabling a channel keeps its note size, font, instrument and transposition, so a later script starts from
    # whatever the same channel held when its earlier scripts yielded; those are known only once they have run.
    def analyze(self):
        self.schedule(SequenceState(0))
        steps = 0
        while self.pending or self.start_reenabled_channels():
            state = self.pending.popleft()
            steps += 1
            if steps > MAX_STEPS:
                self.report("analysis_limit", state.pc, f"stopped after {MAX_STEPS} steps")
                break
            for successor in self.step(state):
                self.schedule(successor)
        return sorted(self.findings, key=lambda finding: (finding.offset, finding.kind))

    def enable_channel(self, index, target):
        if index in self.enabled_channels:
            self.reenables.add((index, target))
            return []
        self.enabled_channels.add(index)
        return [ChannelState(target, self.fonts[-1] if self.fonts else None, index)]

    def start_reenabled_channels(self):
        for index, target in sorted(self.reenables):
            exits = self.channel_exits.get(index) or {ChannelState(0, self.fonts[-1] if self.fonts else None, index)}
            for exit_state in exits:
                self.schedule(replace(exit_state, pc=target))
        return bool(self.pending)

    def record_channel_exit(self, state):
        self.channel_exits.setdefault(state.index, set()).add(replace(state, pc=0, stack=(), started_layers=()))

    def schedule(self, state):
        if state in self.visited:
            return
        self.visited.add(state)
        self.pending.append(state)

    def report(self, kind, offset, detail):
        self.findings.add(Finding(kind, offset, detail))

    def step(self, state):
        handlers = {SequenceState: self.step_sequence, ChannelState: self.step_channel, LayerState: self.step_layer}
        cursor = ScriptCursor(self.data, state.pc)
        try:
            return handlers[type(state)](state, cursor)
        except Truncated:
            self.report("truncated", state.pc, "an instruction runs past the end of the sequence")
            return []

    def is_inside(self, offset, length, write, at):
        if 0 <= offset and offset + length <= len(self.data):
            return True
        kind = "write_out_of_range" if write else "pointer_out_of_range"
        self.report(kind, at, f"offset 0x{offset:X} (+{length}) outside {len(self.data)} bytes")
        return False

    def follow_flow(self, state, cursor, command, ends_on_delay):
        at = state.pc
        argument = cursor.read_table_arguments(command)
        stack = state.stack
        if command == 0xFF:
            return [replace(state, pc=stack[-1], stack=stack[:-1])] if stack else []
        if command in (0xFD, 0xFE):
            if command == 0xFD:
                cursor.read_compressed()
            return [] if ends_on_delay else [replace(state, pc=cursor.pc)]
        if command in (0xFC, 0xF8):
            if len(stack) >= SCRIPT_STACK_DEPTH:
                self.report("stack_overflow", at, f"opcode 0x{command:02X} past depth {SCRIPT_STACK_DEPTH}")
                return []
            target = argument[0] if command == 0xFC else cursor.pc
            if not self.is_inside(target, 1, False, at):
                return []
            return [replace(state, pc=target, stack=stack + (cursor.pc,))]
        if command in (0xF7, 0xF6):
            if not stack:
                self.report("stack_underflow", at, f"opcode 0x{command:02X} with an empty stack")
                return []
            leave = replace(state, pc=cursor.pc, stack=stack[:-1])
            return [replace(state, pc=stack[-1]), leave] if command == 0xF7 else [leave]
        if command in (0xF2, 0xF3, 0xF4):
            target = cursor.pc + to_s8(argument[0])
            taken = [replace(state, pc=target)] if self.is_inside(target, 1, False, at) else []
            return taken if command == 0xF4 else taken + [replace(state, pc=cursor.pc)]
        target = argument[0] if argument else cursor.pc
        taken = [replace(state, pc=target)] if self.is_inside(target, 1, False, at) else []
        return taken if command == 0xFB else taken + [replace(state, pc=cursor.pc)]

    def step_sequence(self, state, cursor):
        command = cursor.read_u8()
        if command >= 0xF2:
            return self.follow_flow(state, cursor, command, False)
        if command >= 0xC0:
            return self.run_sequence_command(state, cursor, command)
        return self.run_sequence_port_command(state, cursor, command)

    def run_sequence_command(self, state, cursor, command):
        if command == 0xC6:
            return []
        arguments = [cursor.read_u8() if size == "b" else cursor.read_u16()
                     for size in SEQUENCE_ARGUMENTS.get(command, "")]
        if command in (0xD1, 0xD2):
            self.is_inside(arguments[0], 16, False, state.pc)
        elif command == 0xC7:
            self.is_inside(arguments[1], 1, True, state.pc)
        elif command == 0xCD:
            self.is_inside(arguments[0], 2, False, state.pc)
        return [replace(state, pc=cursor.pc)]

    def run_sequence_port_command(self, state, cursor, command):
        port = command & 0xF
        group = command & 0xF0
        successors = []
        if group in (0x90, 0xA0):
            target = cursor.read_u16() if group == 0x90 else cursor.read_s16() + cursor.pc
            if self.is_inside(target, 1, False, state.pc):
                successors.extend(self.enable_channel(port, target))
        elif group == 0xB0:
            cursor.read_u8()
            self.is_inside(cursor.read_u16(), 1, False, state.pc)
        elif group == 0x60:
            cursor.read_u8()
            cursor.read_u8()
        if group in (0x60, 0x70, 0xB0) and port >= IO_PORT_COUNT:
            self.report("io_write_out_of_range", state.pc, f"sequence io port {port}")
        if group in (0x50, 0x80) and port >= IO_PORT_COUNT:
            self.report("io_read_out_of_range", state.pc, f"sequence io port {port}")
        return successors + [replace(state, pc=cursor.pc)]

    # Layers run after the channel yields and read its note size, font and instrument live, so they start from the
    # channel's state at the yield rather than at the opcode that started them.
    def step_channel(self, state, cursor):
        command = cursor.read_u8()
        successors = self.run_channel_instruction(state, cursor, command)
        same_channel = [successor for successor in successors
                        if isinstance(successor, ChannelState) and successor.index == state.index]
        is_yield = command < 0x10 or command in (0xFD, 0xFE) or not same_channel
        if not is_yield:
            return successors
        snapshot = same_channel[0] if same_channel else state
        self.record_channel_exit(snapshot)
        layers = [LayerState(pc, replace(snapshot, pc=0, stack=(), started_layers=()))
                  for pc in snapshot.started_layers]
        cleared = [replace(successor, started_layers=()) if isinstance(successor, ChannelState) else successor
                   for successor in successors]
        return cleared + layers

    def run_channel_instruction(self, state, cursor, command):
        if command >= 0xF2:
            return self.follow_flow(state, cursor, command, False)
        if command >= 0xB0:
            return self.run_channel_command(state, cursor, command)
        if command >= 0x70:
            return self.run_channel_layer_command(state, cursor, command)
        return self.run_channel_port_command(state, cursor, command)

    def run_channel_command(self, state, cursor, command):
        at = state.pc
        arguments = cursor.read_table_arguments(command)
        extra = [cursor.read_u8() for _ in range(5)] if command == 0xE8 else []
        if command == 0xEA:
            return []
        if command in (0xC6, 0xEB):
            state = self.switch_font(state, arguments[0], at)
        if command in (0xC1, 0xEB):
            state = self.set_channel_instrument(state, arguments[-1], at)
        if command in (0xC3, 0xC4):
            state = replace(state, large_notes=command == 0xC4)
        if command == 0xE8:
            state = replace(state, transposition=to_s8(extra[0]))
        self.check_channel_pointer(command, arguments, at)
        return [replace(state, pc=cursor.pc)]

    def check_channel_pointer(self, command, arguments, at):
        writes = {0xC7: (1, 1), 0xCF: (0, 2)}
        reads = {0xC2: 2, 0xDA: 1, 0xCB: 1, 0xE7: 8, 0xB0: 16, 0xB2: 2}
        if command in writes:
            index, length = writes[command]
            self.is_inside(arguments[index], length, True, at)
        elif command in reads:
            self.is_inside(arguments[0], reads[command], False, at)
        elif command == 0xCD and arguments[0] >= CHANNEL_COUNT:
            self.report("channel_index_out_of_range", at, f"disables channel {arguments[0]}")

    def run_channel_layer_command(self, state, cursor, command):
        group = command & 0xF8
        if group not in (0x88, 0x78):
            return [replace(state, pc=cursor.pc)]
        offset = cursor.read_s16()
        target = offset & 0xFFFF if group == 0x88 else cursor.pc + offset
        if not self.is_inside(target, 1, False, state.pc):
            return [replace(state, pc=cursor.pc)]
        return [replace(state, pc=cursor.pc, started_layers=state.started_layers + (target,))]

    def run_channel_port_command(self, state, cursor, command):
        port = command & 0xF
        group = command & 0xF0
        successors = []
        if group == 0x20:
            target = cursor.read_u16()
            if self.is_inside(target, 1, False, state.pc):
                successors.extend(self.enable_channel(port, target))
        elif group in (0x30, 0x40):
            target_port = cursor.read_u8()
            if target_port >= IO_PORT_COUNT:
                kind = "io_write_out_of_range" if group == 0x30 else "io_read_out_of_range"
                self.report(kind, state.pc, f"channel {port} io port {target_port}")
        elif group in (0x50, 0x60) and port >= IO_PORT_COUNT:
            self.report("io_read_out_of_range", state.pc, f"channel io port {port}")
        return [replace(state, pc=cursor.pc)] + successors

    def switch_font(self, state, index, at):
        if index >= len(self.fonts):
            self.report("font_index_out_of_range", at, f"font index {index} with {len(self.fonts)} listed")
            return state
        return replace(state, font=self.fonts[len(self.fonts) - index - 1])

    def set_channel_instrument(self, state, value, at):
        mode = instrument_mode(value)
        if mode == "instrument":
            self.check_entry("instrument_missing", state.font, value, at)
        return replace(state, instrument=mode)

    def check_entry(self, kind, font_id, index, at):
        font = self.font_table.get(font_id)
        if font is None:
            self.report("unknown_font", at, f"font {font_id} is not an OoT font")
            return
        entries = {"instrument_missing": font.instruments, "drum_missing": font.drums,
                   "sfx_missing": font.sound_effects}[kind]
        if index >= len(entries) or not entries[index]:
            self.report(kind, at, f"font {font_id} entry {index} of {len(entries)}")

    def step_layer(self, state, cursor):
        command = cursor.read_u8()
        if command >= 0xF2:
            return self.follow_flow(state, cursor, command, True)
        if command >= 0xC1:
            return [self.run_layer_command(state, cursor, command)]
        if command == 0xC0:
            cursor.read_compressed()
            return [replace(state, pc=cursor.pc)]
        self.read_note(state, cursor, command)
        self.check_note(state, command & 0x3F)
        return [replace(state, pc=cursor.pc)]

    def run_layer_command(self, state, cursor, command):
        at = state.pc
        if command in LAYER_BYTE_ARGUMENTS:
            value = cursor.read_u8()
            if command == 0xC2:
                state = replace(state, transposition=to_s8(value))
        elif command == 0xC3:
            cursor.read_compressed()
        elif command == 0xC6:
            state = self.set_layer_instrument(state, cursor.read_u8(), at)
        elif command == 0xC7:
            mode = cursor.read_u8()
            cursor.read_u8()
            cursor.read_u8() if mode & 0x80 else cursor.read_compressed()
        elif command == 0xCB:
            self.is_inside(cursor.read_u16(), 1, False, at)
            cursor.read_u8()
        return replace(state, pc=cursor.pc)

    def set_layer_instrument(self, state, value, at):
        if value == 0xFF:
            return replace(state, instrument="channel")
        mode = instrument_mode(value)
        if mode == "instrument":
            self.check_entry("instrument_missing", state.channel.font, value, at)
        return replace(state, instrument=mode)

    def read_note(self, state, cursor, command):
        kind = command & 0xC0
        if kind in (0x00, 0x40) and (state.channel.large_notes or kind == 0x00):
            cursor.read_compressed()
        if state.channel.large_notes:
            cursor.read_u8()
            if kind != 0x40:
                cursor.read_u8()

    def check_note(self, state, semitone):
        mode = state.channel.instrument if state.instrument == "channel" else state.instrument
        if mode == "drum":
            drum = (semitone + state.channel.transposition + state.transposition) & 0xFF
            self.check_entry("drum_missing", state.channel.font, drum, state.pc)
        elif mode == "sfx":
            sound_effect = ((state.transposition << 6) + semitone) & 0xFFFF
            self.check_entry("sfx_missing", state.channel.font, sound_effect, state.pc)


class ResourceReader:
    def __init__(self, blob):
        self.blob = blob
        self.offset = RESOURCE_HEADER_SIZE

    def take(self, size):
        if self.offset + size > len(self.blob):
            raise ValueError("resource ends early")
        chunk = self.blob[self.offset:self.offset + size]
        self.offset += size
        return chunk

    def u8(self):
        return self.take(1)[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def skip_string(self):
        self.take(self.u32())

    def skip_sound(self):
        if not self.u8():
            return False
        self.u8()
        length = self.u32()
        has_sample = length > 0
        self.take(length)
        self.take(4)
        return has_sample


def parse_sequence_resource(blob):
    reader = ResourceReader(blob)
    data = reader.take(reader.u32())
    reader.take(3)
    font_count = reader.u32()
    if font_count == STREAMED_FONT_COUNT:
        return data, None
    return data, list(reader.take(font_count))


def parse_font_resource(blob):
    reader = ResourceReader(blob)
    font_id = struct.unpack("<i", reader.take(4))[0]
    reader.take(8)
    drum_count, instrument_count, sound_effect_count = reader.u32(), reader.u32(), reader.u32()
    drums = []
    for _ in range(drum_count):
        reader.take(3)
        reader.take(reader.u32() * 4)
        reader.u8()
        length = reader.u32()
        reader.take(length + 4)
        drums.append(length > 0)
    instruments = []
    for _ in range(instrument_count):
        is_valid = reader.u8()
        reader.take(4)
        reader.take(reader.u32() * 4)
        for _ in range(3):
            reader.skip_sound()
        instruments.append(bool(is_valid))
    sound_effects = [reader.skip_sound() for _ in range(sound_effect_count)]
    return font_id, Font(tuple(instruments), tuple(drums), tuple(sound_effects))


def load_font_table(archive_path):
    table = {}
    with zipfile.ZipFile(archive_path) as archive:
        for name in archive.namelist():
            if name.startswith(FONT_PREFIX):
                font_id, font = parse_font_resource(archive.read(name))
                table[font_id] = font
    return table


def decode_name(raw):
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def read_mpq_sequences(path):
    import mpyq

    archive = mpyq.MPQArchive(str(path), listfile=True)
    for raw_name in archive.files:
        if raw_name.startswith(CUSTOM_MUSIC_PREFIX):
            yield decode_name(raw_name), archive.read_file(raw_name)


def read_zip_sequences(path):
    with zipfile.ZipFile(path) as archive:
        for name in archive.namelist():
            if name.encode("utf-8").startswith(CUSTOM_MUSIC_PREFIX):
                yield name, archive.read(name)


def read_pack_sequences(path):
    if path.suffix.lower() in (".otr", ".mpq"):
        return read_mpq_sequences(path)
    return read_zip_sequences(path)


def check_sequence(blob, font_table):
    try:
        data, fonts = parse_sequence_resource(blob)
    except ValueError as error:
        return [Finding("malformed_resource", 0, str(error))]
    findings = []
    if fonts is None:
        fonts = []
    if len(fonts) > MAX_SEQUENCE_FONTS:
        findings.append(Finding("too_many_fonts", 0, f"{len(fonts)} fonts listed, {MAX_SEQUENCE_FONTS} fit"))
    return findings + SequenceAnalyzer(data, fonts, font_table).analyze()


def find_packs(paths):
    packs = []
    for path in paths:
        if path.is_dir():
            packs.extend(sorted(child for child in path.iterdir() if child.suffix.lower() in (".otr", ".o2r")))
        else:
            packs.append(path)
    return packs


def check_packs(packs, font_table):
    results = {}
    for pack in packs:
        for name, blob in read_pack_sequences(pack):
            if blob is None:
                results[(pack.name, name)] = [Finding("malformed_resource", 0, "entry could not be read")]
                continue
            results[(pack.name, name)] = check_sequence(blob, font_table)
    return results


def print_report(results, verbose):
    sequences_by_kind = Counter()
    for findings in results.values():
        for kind in {finding.kind for finding in findings}:
            sequences_by_kind[kind] += 1
    print(f"{len(results)} sequences checked")
    for kind, count in sorted(sequences_by_kind.items(), key=lambda item: (item[0] not in UNSAFE_KINDS, item[0])):
        label = "unsafe" if kind in UNSAFE_KINDS else "silent"
        print(f"  {label} {kind}: {count} sequences")
    ranked = sorted(results.items(), key=lambda item: -sum(f.kind in UNSAFE_KINDS for f in item[1]))
    for (pack, name), findings in ranked:
        shown = findings if verbose else [finding for finding in findings if finding.kind in UNSAFE_KINDS]
        if not shown:
            continue
        print(f"{pack} :: {name}")
        for finding in shown:
            print(f"    0x{finding.offset:04X} {finding.kind}: {finding.detail}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("packs", nargs="+", type=Path, help=".otr/.o2r packs or folders holding them")
    parser.add_argument("--fonts", type=Path, required=True, help="archive with OoT's audio/fonts, e.g. oot.o2r")
    parser.add_argument("--verbose", action="store_true", help="also list silent findings")
    arguments = parser.parse_args()
    results = check_packs(find_packs(arguments.packs), load_font_table(arguments.fonts))
    print_report(results, arguments.verbose)
    has_unsafe = any(finding.kind in UNSAFE_KINDS for findings in results.values() for finding in findings)
    return 1 if has_unsafe else 0


if __name__ == "__main__":
    sys.exit(main())
