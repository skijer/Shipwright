import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import sign_request

# The queue lives in issue labels and in markers inside the workflow's own comments, so a run that is cancelled
# or fails never loses a request. Each modder keeps one issue open for good: it is their mods' page in the store.
LABEL_REQUEST = "sign-mod"
LABEL_QUEUED = "sign-queued"
LABEL_AWAITING = "awaiting-approval"
LABEL_APPROVED = "approved"
LABEL_SIGNED = "signed"
LABEL_FAILED = "sign-failed"
REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TRUSTED_AUTHORS = REPOSITORY_ROOT / ".github" / "mod-signing" / "trusted-authors.txt"
REVOKED_MODS = REPOSITORY_ROOT / "soh" / "soh" / "ModApi" / "ModTrust" / "RevokedMods.h"
UNBOUND_REF = re.compile(r"^[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)*$")
WORKFLOW_FILE = "sign-mod.yml"
STORE_RELEASE = "mods"
CATALOG_ASSET = "catalog.json"
MAINTAINER_PERMISSIONS = {"admin", "maintain", "write"}
AWAITING_MESSAGE = ("Thanks! This is your first signature request, so a maintainer reviews it before anything is "
                    "built. Signing starts as soon as they add the `approved` label.")
MOVED_MESSAGE = ("The repository or mods folder of this issue changed after it was approved, so it waits for a "
                 "maintainer to review it again and add `approved`.")
REVOKED_MESSAGE = "This account can no longer get mods signed by Unbound."


def read_trusted_authors(path=TRUSTED_AUTHORS):
    lines = path.read_text(encoding="utf-8").splitlines() if path.is_file() else []
    return {line.strip().lower() for line in lines if line.strip() and not line.strip().startswith("#")}


def read_revoked_authors(path=REVOKED_MODS):
    block = re.search(r"kRevokedModAuthors\s*=\s*\{(.*?)\};", path.read_text(encoding="utf-8"), re.DOTALL)
    return {author.lower() for author in re.findall(r'"([^"]+)"', block.group(1))} if block else set()


def triage(issue, trusted, revoked):
    """The label change an untriaged request needs: 'queue', 'await' or 'reject', or None when it waits as is."""
    labels = issue["labels"]
    author = issue["author"].lower()
    if LABEL_QUEUED in labels:
        return None
    if author in revoked:
        return "reject"
    if LABEL_AWAITING in labels and LABEL_APPROVED not in labels:
        return None
    if author in trusted or LABEL_APPROVED in labels:
        return "queue" if LABEL_AWAITING in labels or not has_signed(issue) else None
    return "await"


def has_signed(issue):
    return LABEL_SIGNED in issue["labels"] or LABEL_FAILED in issue["labels"]


def gh(*arguments):
    return subprocess.run(["gh", *arguments], check=True, capture_output=True, text=True).stdout


FIELDS = "number,author,labels,body,comments"


def read_issue(issue):
    comments = [{"author": (comment.get("author") or {}).get("login", ""), "body": comment.get("body") or ""}
                for comment in issue.get("comments", [])]
    return {"number": issue["number"], "author": issue["author"]["login"], "body": issue["body"] or "",
            "labels": {label["name"] for label in issue["labels"]}, "comments": comments}


def list_open_requests(repository):
    output = gh("issue", "list", "--repo", repository, "--label", LABEL_REQUEST, "--state", "open", "--limit", "200",
                "--json", FIELDS)
    return [read_issue(issue) for issue in json.loads(output)]


def get_issue(repository, number):
    return read_issue(json.loads(gh("issue", "view", str(number), "--repo", repository, "--json", FIELDS)))


def edit_labels(repository, issue, add=(), remove=()):
    arguments = ["issue", "edit", str(issue["number"]), "--repo", repository]
    for label in add:
        arguments += ["--add-label", label]
    for label in remove:
        if label in issue["labels"]:
            arguments += ["--remove-label", label]
    gh(*arguments)
    issue["labels"] = (issue["labels"] - set(remove)) | set(add)


def comment(repository, issue, body):
    gh("issue", "comment", str(issue["number"]), "--repo", repository, "--body", body)
    issue.setdefault("comments", []).append({"author": "github-actions", "body": body})


def close_as(repository, issue, label, message):
    edit_labels(repository, issue, add=[label], remove=[LABEL_QUEUED, LABEL_AWAITING])
    comment(repository, issue, message)
    gh("issue", "close", str(issue["number"]), "--repo", repository)


def fail(repository, issue, message):
    edit_labels(repository, issue, add=[LABEL_FAILED], remove=[LABEL_QUEUED, LABEL_SIGNED])
    comment(repository, issue, f"Not signed: {message}")


def queue(repository, issue, commit, note, extra=None):
    marker = sign_request.write_marker("sign", {"commit": commit, **(extra or {})})
    comment(repository, issue, f"{note}\n\n{marker}")
    edit_labels(repository, issue, add=[LABEL_QUEUED], remove=[LABEL_AWAITING])


def queued_commits(issue):
    return {marker.get("commit") for marker in sign_request.read_markers(issue["comments"], "sign")}


def apply_triage(repository, issue, action):
    if action == "queue":
        request = sign_request.parse_issue_body(issue["body"])
        markers = sign_request.read_markers(issue["comments"], "sign")
        commit = markers[-1]["commit"] if markers else request.get("commit", "")
        if not sign_request.PATTERNS["commit"].match(commit):
            fail(repository, issue, "the issue names no commit to sign; comment `/sign <tag or commit>`")
            return
        queue(repository, issue, commit, f"Queued `{commit[:7]}` for signing.")
    elif action == "await":
        edit_labels(repository, issue, add=[LABEL_AWAITING])
        comment(repository, issue, AWAITING_MESSAGE)
    elif action == "reject":
        close_as(repository, issue, LABEL_FAILED, REVOKED_MESSAGE)


def load_catalog(repository):
    """The store catalog's payload, or an empty one before the first signature. The sign job verifies it."""
    with tempfile.TemporaryDirectory() as directory:
        try:
            gh("release", "download", STORE_RELEASE, "--repo", repository, "--pattern", CATALOG_ASSET,
               "--dir", directory)
        except subprocess.CalledProcessError:
            return {}
        envelope = json.loads((Path(directory) / CATALOG_ASSET).read_text(encoding="utf-8"))
    return json.loads(envelope.get("payload", "{}"))


def resolve_reference(repository, reference):
    return gh("api", f"repos/{repository}/commits/{reference}", "--jq", ".sha").strip()


def is_reference_of(repository, reference):
    try:
        resolve_reference(repository, reference)
        return True
    except subprocess.CalledProcessError:
        return False


def is_maintainer(repository, user):
    try:
        permission = gh("api", f"repos/{repository}/collaborators/{user}/permission", "--jq", ".permission")
    except subprocess.CalledProcessError:
        return False
    return permission.strip() in MAINTAINER_PERMISSIONS


def can_sign(issue, trusted):
    return issue["author"].lower() in trusted or LABEL_APPROVED in issue["labels"]


def pick_unbound_ref(repository, request):
    """The mods' UNBOUND_REF, else the newest Unbound release, else the commit this workflow runs from."""
    try:
        reference = gh("api", "-H", "Accept: application/vnd.github.raw",
                       f"repos/{request['repository']}/contents/UNBOUND_REF?ref={request['commit']}").strip()
    except subprocess.CalledProcessError:
        reference = ""
    if not reference:
        releases = json.loads(gh("release", "list", "--repo", repository, "--limit", "50", "--json", "tagName"))
        reference = next((release["tagName"] for release in releases if "unbound" in release["tagName"]), "")
    if not reference:
        return os.environ["GITHUB_SHA"]
    if not UNBOUND_REF.match(reference) or not is_reference_of(repository, reference):
        raise ValueError(f"'{reference}' is not a tag, branch or commit of {repository}")
    return reference


def check_submitted_repository(request, author):
    details = json.loads(gh("api", f"repos/{request['repository']}"))
    if details.get("private", True):
        raise ValueError(f"{request['repository']} must be public so anyone can read what was signed")
    if details["owner"]["login"].lower() != author.lower():
        raise ValueError(f"only {details['owner']['login']} can request signatures for {request['repository']}")
    commit = json.loads(gh("api", f"repos/{request['repository']}/commits/{request['commit']}"))
    if commit.get("sha") != request["commit"]:
        raise ValueError(f"{request['commit']} is not a commit of {request['repository']}")


def write_outputs(values):
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        for key, value in values.items():
            output.write(f"{key}={value}\n")


def handle_command(repository, issue, body, commenter, trusted):
    """Acts on a /sign, /unlist or /relist comment; a catalog change is returned for the sign job."""
    try:
        command = sign_request.parse_command(body)
    except ValueError as error:
        comment(repository, issue, str(error))
        return None
    if command is None:
        return None
    if commenter.lower() != issue["author"].lower() and not is_maintainer(repository, commenter):
        return None

    name, argument = command
    if name != "sign":
        return {"mode": "catalog", "issue": issue["number"], "command": f"{name}:{argument}", "author": issue["author"]}
    request = sign_request.parse_issue_body(issue["body"])
    try:
        commit = resolve_reference(request.get("repository", ""), argument)
    except subprocess.CalledProcessError:
        comment(repository, issue, f"`{argument}` is not a tag, branch or commit of {request.get('repository')}.")
        return None
    if can_sign(issue, trusted):
        queue(repository, issue, commit, f"Queued `{argument}` (`{commit[:7]}`) for signing.")
    else:
        marker = sign_request.write_marker("sign", {"commit": commit})
        comment(repository, issue, f"`{commit[:7]}` will be signed once a maintainer approves this issue.\n\n{marker}")
    return None


def recheck_approval(repository, issue, catalog, trusted):
    """An approval covers the repository and folder it was given for, not whatever the body says later."""
    source = catalog.get("sources", {}).get(str(issue["number"]))
    if source is None or issue["author"].lower() in trusted or LABEL_APPROVED not in issue["labels"]:
        return
    request = sign_request.parse_issue_body(issue["body"])
    if (request.get("repository"), request.get("directory")) != (source["repository"], source["directory"]):
        edit_labels(repository, issue, add=[LABEL_AWAITING], remove=[LABEL_APPROVED, LABEL_QUEUED])
        comment(repository, issue, MOVED_MESSAGE)


def latest_release_commit(repository):
    tag = gh("api", f"repos/{repository}/releases/latest", "--jq", ".tag_name").strip()
    return tag, resolve_reference(repository, tag)


def follows_releases(source, repository):
    """Mods kept inside the Unbound repository share its releases, which are the game's, not theirs."""
    return source["repository"].lower() != repository.lower()


def queue_new_releases(repository, issues, catalog, trusted):
    for issue in issues:
        source = catalog.get("sources", {}).get(str(issue["number"]))
        if source is None or LABEL_QUEUED in issue["labels"] or not can_sign(issue, trusted):
            continue
        if not follows_releases(source, repository):
            continue
        try:
            tag, commit = latest_release_commit(source["repository"])
        except subprocess.CalledProcessError:
            continue
        if commit != source["commit"] and commit not in queued_commits(issue):
            queue(repository, issue, commit, f"New release `{tag}` of {source['repository']}: queued for signing.")


def queue_resign(repository, issues, catalog, unbound_ref, sdk_run):
    """Rebuilds every listed source against another Unbound, at the commit that was last signed."""
    for issue in issues:
        source = catalog.get("sources", {}).get(str(issue["number"]))
        if source is None:
            continue
        extra = {"unbound_ref": unbound_ref, "sdk_run": sdk_run} if sdk_run else {"unbound_ref": unbound_ref}
        queue(repository, issue, source["commit"], f"Rebuilding for Unbound `{unbound_ref[:12]}`.", extra)


def read_event():
    path = os.environ.get("GITHUB_EVENT_PATH", "")
    return json.loads(Path(path).read_text(encoding="utf-8")) if path and Path(path).is_file() else {}


def build_request(repository, issue, trusted):
    request = sign_request.parse_issue_body(issue["body"])
    markers = sign_request.read_markers(issue["comments"], "sign")
    marker = markers[-1] if markers else {}
    request["commit"] = marker.get("commit", request.get("commit", ""))
    request = sign_request.validate_request(request)
    check_submitted_repository(request, issue["author"])
    unbound_ref = marker.get("unbound_ref") or pick_unbound_ref(repository, request)
    if not UNBOUND_REF.match(unbound_ref):
        raise ValueError(f"'{unbound_ref}' is not a usable Unbound reference")
    held = sign_request.read_markers(issue["comments"], "held")
    return {"mode": "build", "issue": issue["number"], "author": issue["author"], "unbound_ref": unbound_ref,
            "sdk_run": marker.get("sdk_run", ""), "trusted": str(issue["author"].lower() in trusted).lower(),
            "approved": str(LABEL_APPROVED in issue["labels"]).lower(),
            "held_permissions": held[-1].get("permissions", "") if held else "", **request}


def next_request(repository):
    trusted = read_trusted_authors()
    revoked = read_revoked_authors()
    catalog = load_catalog(repository)
    issues = list_open_requests(repository)
    event_name = os.environ.get("GITHUB_EVENT_NAME", "")
    event = read_event()

    if event_name == "issue_comment":
        number = event.get("issue", {}).get("number")
        issue = next((issue for issue in issues if issue["number"] == number), None)
        if issue is not None:
            change = handle_command(repository, issue, event["comment"].get("body") or "",
                                    event["comment"]["user"]["login"], trusted)
            if change is not None:
                return change
    if event_name == "workflow_dispatch" and os.environ.get("RESIGN_ALL") == "true":
        queue_resign(repository, issues, catalog, os.environ["RESIGN_UNBOUND_REF"], os.environ.get("RESIGN_SDK_RUN", ""))
    if event_name == "schedule":
        queue_new_releases(repository, issues, catalog, trusted)

    for issue in issues:
        recheck_approval(repository, issue, catalog, trusted)
        apply_triage(repository, issue, triage(issue, trusted, revoked))

    for issue in sorted((issue for issue in issues if LABEL_QUEUED in issue["labels"]), key=lambda i: i["number"]):
        try:
            request = build_request(repository, issue, trusted)
        except (ValueError, subprocess.CalledProcessError) as error:
            fail(repository, issue, str(error))
            continue
        print(f"Signing #{issue['number']}: {request['repository']}@{request['commit']}")
        return request
    print("No signature request is waiting")
    return {"mode": "", "issue": ""}


def finish_request(repository, number, outcome, summary, held, run_url):
    issue = get_issue(repository, number)
    if held:
        marker = sign_request.write_marker("held", {"permissions": held})
        edit_labels(repository, issue, add=[LABEL_AWAITING], remove=[LABEL_APPROVED, LABEL_QUEUED])
        comment(repository, issue, f"{summary}\n\nA maintainer has to approve the new permissions ({held}): "
                                   f"they add `approved` again to publish them. ({run_url})\n\n{marker}")
    elif outcome == "signed":
        edit_labels(repository, issue, add=[LABEL_SIGNED], remove=[LABEL_QUEUED, LABEL_FAILED])
        comment(repository, issue, f"{summary}\n\n[Scan and build logs]({run_url})")
    else:
        edit_labels(repository, issue, add=[LABEL_FAILED], remove=[LABEL_QUEUED, LABEL_SIGNED])
        comment(repository, issue, f"Not signed: a check failed. The log of each step says why: {run_url}")
    if any(LABEL_QUEUED in issue["labels"] for issue in list_open_requests(repository)):
        gh("workflow", "run", WORKFLOW_FILE, "--repo", repository)


def main():
    parser = argparse.ArgumentParser(description="Run the sign-mod queue: one request at a time.")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("next", help="Triage requests and hand the oldest queued one to the workflow")
    finish = commands.add_parser("finish", help="Report a request on its issue and start the next one")
    finish.add_argument("--issue", type=int, required=True)
    finish.add_argument("--outcome", choices=("signed", "failed"), required=True)
    finish.add_argument("--summary-file", type=Path)
    finish.add_argument("--held", default="")
    finish.add_argument("--run-url", required=True)
    args = parser.parse_args()

    repository = os.environ["GITHUB_REPOSITORY"]
    if args.command == "next":
        write_outputs(next_request(repository))
    else:
        summary = args.summary_file.read_text(encoding="utf-8") if args.summary_file and args.summary_file.is_file() \
            else "Signed."
        finish_request(repository, args.issue, args.outcome, summary.strip(), args.held, args.run_url)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(f"gh failed: {error.stderr}")
