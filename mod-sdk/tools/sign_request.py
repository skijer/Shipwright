import re

ISSUE_FIELDS = {"Mod repository": "repository", "Commit": "commit", "Mods folder": "directory"}
DEFAULT_DIRECTORY = "mods"
NO_RESPONSE = "_No response_"
PATTERNS = {
    "repository": re.compile(r"^[A-Za-z0-9-]{1,39}/[A-Za-z0-9._-]{1,100}$"),
    "commit": re.compile(r"^[0-9a-f]{40}$"),
    "directory": re.compile(r"^[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)*$"),
}
SECTION = re.compile(r"^### (.+?)[ \t]*\n(.*?)(?=^### |\Z)", re.MULTILINE | re.DOTALL)
COMMAND = re.compile(r"^/(sign|unlist|relist)(?:[ \t]+(\S+))?[ \t]*$", re.MULTILINE)
REFERENCE = re.compile(r"^[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)*$")
MOD_ID = re.compile(r"^[a-z0-9_-]{1,64}$")
MARKER = re.compile(r"<!-- unbound-(?P<kind>[a-z]+)(?P<values>(?: [a-z_]+=[A-Za-z0-9._/,-]*)*) -->")
MARKER_VALUE = re.compile(r"^[A-Za-z0-9._/,-]*$")
# Only comments the workflow wrote carry state; anyone can type a marker into their own comment.
BOT_AUTHORS = {"github-actions", "github-actions[bot]"}


def parse_issue_body(body):
    request = {"directory": DEFAULT_DIRECTORY}
    for heading, value in SECTION.findall(body.replace("\r\n", "\n")):
        field = ISSUE_FIELDS.get(heading.strip())
        value = value.strip()
        if field and value and value != NO_RESPONSE:
            request[field] = value
    if "commit" in request:
        request["commit"] = request["commit"].lower()
    return request


def validate_request(request):
    for field, pattern in PATTERNS.items():
        if not pattern.match(request.get(field, "")):
            raise ValueError(f"'{field}' is missing or malformed")
    if any(part in (".", "..") for part in request["directory"].split("/")):
        raise ValueError("the mods folder must stay inside the repository")
    return request


def parse_command(comment):
    """The first /sign, /unlist or /relist line of a comment as (command, argument), or None."""
    match = COMMAND.search(comment.replace("\r\n", "\n"))
    if match is None:
        return None
    command, argument = match.group(1), match.group(2) or ""
    if command == "sign" and not REFERENCE.match(argument):
        raise ValueError("/sign needs a tag, branch or commit of your repository, e.g. `/sign v1.2.0`")
    if command != "sign" and not MOD_ID.match(argument):
        raise ValueError(f"/{command} needs the folder name of one of your mods, e.g. `/{command} whip`")
    return command, argument


def write_marker(kind, values):
    for value in values.values():
        if not MARKER_VALUE.match(str(value)):
            raise ValueError(f"'{value}' cannot be stored in an issue marker")
    pairs = "".join(f" {key}={value}" for key, value in values.items())
    return f"<!-- unbound-{kind}{pairs} -->"


def read_markers(comments, kind):
    """Markers of one kind from the workflow's own comments, oldest first, as dictionaries."""
    markers = []
    for comment in comments:
        if comment.get("author") not in BOT_AUTHORS:
            continue
        for match in MARKER.finditer(comment.get("body", "")):
            if match["kind"] == kind:
                markers.append(dict(pair.split("=", 1) for pair in match["values"].split()))
    return markers
