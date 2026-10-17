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
