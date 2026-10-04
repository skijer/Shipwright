"""Exercise production hint lookup/insertion with real Item, HintText and Text types.

The fixture substitutes seed storage and final text-box formatting. Lookup,
localized text conversion, clarity selection and substitution run unchanged.
--source-root allows the same regression to exercise the ComboShip port.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    pos = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]


def arguments(source):
    """Split the Item constructor's arguments, respecting Text/initializer lists."""
    start = source.index("Item(") + 5
    depth = 0
    quoted = escaped = False
    result = []
    token = start
    for pos in range(start, len(source)):
        char = source[pos]
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            continue
        if char == '"':
            quoted = True
        elif char in "({[":
            depth += 1
        elif char == ")" and depth == 0:
            result.append(source[token:pos].strip())
            return result
        elif char in ")}]":
            depth -= 1
        elif char == "," and depth == 0:
            result.append(source[token:pos].strip())
            token = pos + 1
    raise ValueError("Unterminated Item constructor")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    root = args.source_root.resolve()
    rando = root / "soh/soh/Enhancements/randomizer"
    custom = root / "soh/soh/Enhancements/custom-message"
    item = (rando / "item.cpp").read_text()
    hints = (rando / "3drando/hints.cpp").read_text()
    hint = (rando / "hint.cpp").read_text()
    message = (custom / "CustomMessageManager.cpp").read_text()
    parts = []
    for sig in (
        "CustomMessage::CustomMessage(std::string english_, std::string german_, std::string french_, TextBoxType",
        "CustomMessage::CustomMessage(std::string english_, TextBoxType",
        "CustomMessage::CustomMessage(Text text,",
        "const std::string CustomMessage::GetEnglish(",
        "const std::string CustomMessage::GetGerman(",
        "const std::string CustomMessage::GetFrench(",
        "const std::string CustomMessage::GetForLanguage(",
        "void CustomMessage::Replace(std::string&& oldStr, CustomMessage",
        "void CustomMessage::InsertNames(",
        "void CustomMessage::Capitalize(",
    ):
        parts.append(function(message, sig))
    for sig in (
        "HintText::HintText(", "const CustomMessage& HintText::GetClear()",
        "const CustomMessage& HintText::GetObscure(size_t",
        "const CustomMessage& HintText::GetAmbiguous(size_t",
        "const CustomMessage& HintText::GetHintMessage(",
    ):
        parts.append(function(hints, sig))
    parts.append("namespace Rando {")
    # Both Item constructor overloads share their signature prefix.
    constructors = [m.start() for m in re.finditer(r"Item::Item\(const RandomizerGet", item)]
    parts.append(function(item[constructors[1]:], "Item::Item("))
    for sig in ("Item::Item()", "const Text& Item::GetName()", "const Text& Item::GetArticle()",
                "ItemType Item::GetItemType()", "RandomizerGet Item::GetRandomizerGet()",
                "RandomizerHintTextKey Item::GetHintKey()", "const HintText& Item::GetHint()"):
        parts.append(function(item, sig))
    for sig in ("const HintText Hint::GetItemHintText(", "const CustomMessage Hint::GetItemName("):
        parts.append(function(hint, sig))
    parts.append("}")

    # Real item registrations supply localized names/articles and hint keys.
    rows = []
    for line in (rando / "item_list.cpp").read_text().splitlines():
        if line.lstrip().startswith("//") or not re.search(r"itemTable\[RG_\w+\]\s*=\s*Item\(", line):
            continue
        fields = arguments(line)
        if fields[6] != "RHT_NONE":
            continue
        article = fields[15] if len(fields) > 15 else (fields[8] if 8 < len(fields) < 15 else "{}")
        rows.append("{" + fields[0] + ", Item(" + ", ".join(
            [*fields[:3], "0", "false", "LOGIC_NONE", fields[6], "ITEM_CATEGORY_MAJOR", article]) + ")},")

    flags = ["-std=c++20", "-O0", "-g", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0"]
    flags += ["-I" + str(root / p) for p in ("soh", "soh/include", "soh/src", "soh/assets",
                                              "libultraship/include")]
    if (root / "combo").is_dir():
        flags += ["-DCOMBO_BUILD"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    with tempfile.TemporaryDirectory(prefix="hint-item-names-") as temp:
        folder = Path(temp)
        (folder / "hint_item_name_production.inc").write_text("\n".join(parts))
        (folder / "hint_item_name_rows.inc").write_text("\n".join(rows))
        binary = folder / "test"
        subprocess.run([os.environ.get("CXX", "c++"), *flags, "-I" + temp,
                        str(ROOT / "soh/tests/hint_item_name_test.cpp"), str(custom / "text.cpp"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
