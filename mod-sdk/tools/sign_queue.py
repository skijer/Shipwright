import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import sign_request

# The queue lives in issue labels, so a run that is cancelled or fails never loses a request.
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
AWAITING_MESSAGE = ("Thanks! This is your first signature request, so a maintainer reviews it before anything is "
                    "built. Signing starts as soon as they add the `approved` label.")
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
    if author in trusted or LABEL_APPROVED in labels:
        return "queue"
    return None if LABEL_AWAITING in labels else "await"


def gh(*arguments):
    return subprocess.run(["gh", *arguments], check=True, capture_output=True, text=True).stdout


FIELDS = "number,author,labels,body"


def read_issue(issue):
    return {"number": issue["number"], "author": issue["author"]["login"], "body": issue["body"] or "",
            "labels": {label["name"] for label in issue["labels"]}}


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


def close_as(repository, issue, label, message):
    edit_labels(repository, issue, add=[label], remove=[LABEL_QUEUED, LABEL_AWAITING])
    comment(repository, issue, message)
    gh("issue", "close", str(issue["number"]), "--repo", repository)


def apply_triage(repository, issue, action):
    if action == "queue":
        edit_labels(repository, issue, add=[LABEL_QUEUED], remove=[LABEL_AWAITING])
    elif action == "await":
        edit_labels(repository, issue, add=[LABEL_AWAITING])
        comment(repository, issue, AWAITING_MESSAGE)
    elif action == "reject":
        close_as(repository, issue, LABEL_FAILED, REVOKED_MESSAGE)


def is_reference_of(repository, reference):
    try:
        gh("api", f"repos/{repository}/commits/{reference}", "--jq", ".sha")
        return True
    except subprocess.CalledProcessError:
        return False


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


def next_request(repository):
    trusted = read_trusted_authors()
    revoked = read_revoked_authors()
    issues = list_open_requests(repository)
    for issue in issues:
        apply_triage(repository, issue, triage(issue, trusted, revoked))

    for issue in sorted((issue for issue in issues if LABEL_QUEUED in issue["labels"]), key=lambda i: i["number"]):
        try:
            request = sign_request.validate_request(sign_request.parse_issue_body(issue["body"]))
            check_submitted_repository(request, issue["author"])
            unbound_ref = pick_unbound_ref(repository, request)
        except (ValueError, subprocess.CalledProcessError) as error:
            close_as(repository, issue, LABEL_FAILED, f"Not signed: {error}")
            continue
        print(f"Signing #{issue['number']}: {request['repository']}@{request['commit']}")
        return {"issue": issue["number"], "author": issue["author"], "unbound_ref": unbound_ref, **request}
    print("No signature request is waiting")
    return {"issue": ""}


def finish_request(repository, number, signed, release_tag, run_url):
    if signed:
        server = os.environ.get("GITHUB_SERVER_URL", "https://github.com")
        message = f"Signed. Download the packages from {server}/{repository}/releases/tag/{release_tag} ({run_url})."
    else:
        message = f"Not signed: a check failed. The log of each step says why: {run_url}"
    close_as(repository, get_issue(repository, number), LABEL_SIGNED if signed else LABEL_FAILED, message)
    if any(LABEL_QUEUED in issue["labels"] for issue in list_open_requests(repository)):
        gh("workflow", "run", WORKFLOW_FILE, "--repo", repository)


def main():
    parser = argparse.ArgumentParser(description="Run the sign-mod queue: one request at a time.")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("next", help="Triage new requests and hand the oldest queued one to the workflow")
    finish = commands.add_parser("finish", help="Close a request and start the next one")
    finish.add_argument("--issue", type=int, required=True)
    finish.add_argument("--signed", action="store_true")
    finish.add_argument("--release-tag", default="")
    finish.add_argument("--run-url", required=True)
    args = parser.parse_args()

    repository = os.environ["GITHUB_REPOSITORY"]
    if args.command == "next":
        write_outputs(next_request(repository))
    else:
        finish_request(repository, args.issue, args.signed, args.release_tag, args.run_url)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(f"gh failed: {error.stderr}")
