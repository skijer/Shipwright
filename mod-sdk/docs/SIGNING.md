# Signing

A mod's binary runs inside the game with the same rights as the game. A build with the `signed` policy only
loads a binary that the **sign-mod** workflow of the Unbound repository built and signed; anything else is
refused and listed when the game starts. Asset-only packages (no `binaries` in the manifest) are not affected.

## Three kinds of build

| `UNBOUND_MOD_POLICY` | What loads |
| --- | --- |
| `open` (current default) | Every mod, as before signing existed; the Mod Menu still shows which are signed |
| `ask` | Signed mods, plus unsigned ones after the player says yes |
| `signed` | Only mods signed by the sign-mod workflow |

Set it with `cmake ... -DUNBOUND_MOD_POLICY=signed`. The policy is fixed when the game is compiled: no setting,
config file or mod can switch it. Releases move to `signed` once the signing key is set up.

With `ask`, an unsigned mod opens a prompt the first time: its name, why it is not signed, anything its
binary imports from the system beyond what mods need, and its content hash. "Always load this version" is
remembered by that hash in `mods/.unbound-mod-approvals.json`; changing a single byte of the package asks again.

## Getting a mod signed

1. Push your mods to a **public** repository you own, laid out as in [Publishing](PUBLISHING.md): one folder
   per mod with `manifest.json`, its sources and `assets/`. Add `UNBOUND_REF` to pin the Unbound release (or
   tag, branch or commit) to build against; without it the newest Unbound release is used, or the current
   Unbound commit while there is none.
2. Open a **Sign a mod** issue in the Unbound repository with the repository, the full commit hash and the
   mods folder. Keep it open: it is your mods' page in the in-game **NEI-Store** (Network > NEI-Store).
3. The workflow answers on the issue: the signed mods, now in the store with their download links, or the step
   that failed.

Requests are signed one at a time, oldest first. Your first request waits until a maintainer approves it
(label `approved`); once you are on the trusted list, later ones go straight to the queue.

### Updates

After the first approval, a new version needs no new issue:

- publish a GitHub release in your repository (the workflow checks every two hours), or
- comment `/sign <tag, branch or commit>` on your issue.

Only mods whose sources changed are signed again; the rest keep their published build. A new version that asks
for a permission its mod did not have before waits for a maintainer to add `approved` again. Changing the
repository or folder in the issue after approval also waits for a new approval.

`/unlist <mod>` takes one of your mods out of the store and `/relist <mod>` brings it back. A mod's name is its
folder name, and the first issue that publishes a name keeps it.

### Packs and dependencies

A `packs.json` next to your mod folders groups them; players install a whole pack or only the parts they tick:

```json
{ "packs": [ { "id": "my-items", "name": "My Items", "mods": ["whip", "lantern"] } ] }
```

A mod that needs another one names it in its manifest, and the store installs both:

```json
"dependencies": ["mm_assets"]
```

### Unbound versions

`SOH_MOD_API_MAJOR` in `soh/soh/ModApi/ModApi.h` goes up only when a change breaks mods built before it; adding
entries to `SOHModApi` does not. The SDK stamps it into every package, and the game refuses packages built for
another major. When a release bumps it, `publish-release.yml` rebuilds every listed mod at its last signed commit
before the release goes public, and reports each result on its issue.

The signature names your GitHub account, the repository and the commit, and links to the workflow run with
every scan and build log. The game shows it in the Mod Menu next to the file name.

## Permissions

A mod that needs the microphone or the player's files declares it in its manifest, and gets them only through
the game's services:

```json
"permissions": ["microphone"]
```

| Permission | Services | What the player sees |
| --- | --- | --- |
| `microphone` | `OpenMicrophone`, `ReadMicrophone`, `CloseMicrophone` | Asked once per mod (the answer is kept in `mods/.unbound-mod-permissions.json`); a microphone icon with the mod's name stays in the corner while it records |
| `files` | `PickUserFile`, `SaveToModsFolder`, and the older `ExportArchiveFiles` | Asked every time: a file is only read once the player picks it, and nothing is written without a yes. Writes only land in `mods/` |

`AskPlayer`, `TellPlayer` and `RequestRestart` need no permission; their dialogs carry the mod's name.

The game identifies which mod is calling each service by the binary the call comes from, so one mod cannot use
another's permission. A service the manifest does not declare is refused at run time, and the scan refuses a
mod whose code calls a service its manifest does not declare. For an unsigned mod, permissions are only what it
claims: its binary can reach the system directly, and its prompt says so.

## What the workflow checks

Nothing of your repository runs while it is checked or built: no CMake, scripts or hooks of yours. The build
uses Unbound's own `mod-sdk/ci/signing/CMakeLists.txt`, which compiles each mod folder with the SDK defaults.

| Step | Refuses |
| --- | --- |
| Layout | Build scripts, executables, symbolic links, files other than sources, docs, `manifest.json` and a top-level `packs.json` outside `assets/` |
| Manifest | Unknown permissions; `dependencies` that are not mod folder names; `build` include folders outside the mod, and definitions other than `NAME` or `NAME=number` |
| Directives | Includes that leave the mod's folder or reach the operating system (`windows.h`, `unistd.h`, `<filesystem>`, `<fstream>`, `<thread>`, sockets…), any `#pragma` but `once`, `pack`, `warning`, `region` and diagnostics |
| Preprocessed code | Inline assembly, process-environment intrinsics, and calls that open files, start processes or threads, load libraries or use the network, even when built by macros |
| [Cppcheck](https://cppcheck.sourceforge.io/) | Any error |
| [Semgrep](https://semgrep.dev/) `p/c` | Any finding of severity ERROR |
| Permissions | A manifest permission the game does not know, and code that calls a service its manifest does not declare |
| Binary imports | Anything a binary imports outside the game, the C/C++ runtime and the CRT start-up, the runtime's file, process and thread functions, and the game's own signing and permission code |

A mod that needs any of those must ask the game for it through the ModApi. If the ModApi has no service for
it, the mod cannot be signed yet: report what is missing.

## What a signature does not prove

The checks catch a mod that reaches outside the game; they do not prove a mod is harmless inside it. A signed
mod can still crash, corrupt a save or misbehave, and a determined author can hide behaviour from any scanner.
The signature makes that author identifiable: their account, repository and exact commit are public and stay
in the package.

## Setting up the workflow (maintainers)

1. `python mod-sdk/tools/mod_signature.py generate-key --private-key-output signing.key` writes the public key
   into `soh/soh/ModApi/ModTrust/TrustedModKey.h`.
2. Create the environment **mod-signing** (Settings, Environments), restrict it to the default branch, and
   store the contents of `signing.key` as its secret `UNBOUND_MOD_SIGNING_KEY`. Delete `signing.key`. Do not
   add required reviewers to it: approval happens on the issue, and a reviewer gate would hold up the queue.
3. Create the labels `sign-mod`, `sign-queued`, `awaiting-approval`, `approved`, `signed` and `sign-failed`.
4. Commit `TrustedModKey.h`, change the default of `UNBOUND_MOD_POLICY` in `soh/CMakeLists.txt` to `signed`
   and publish a release: only builds with that header verify the signatures.

A build whose header is all zeros verifies nothing, so a `signed` build without a key refuses every native mod.

The in-game updater has a key of its own, so a leaked mod key cannot ship an Unbound exe:

5. `python mod-sdk/tools/mod_signature.py generate-key --kind release --private-key-output release.key`
   writes `soh/soh/ModApi/ModTrust/TrustedReleaseKey.h`. Store `release.key` as the secret
   `UNBOUND_RELEASE_SIGNING_KEY` of **mod-signing**, delete the file and commit the header.
6. To release, run **publish-release** from the branch the environment allows, with the id of a successful
   generate-builds run less than a day old (artifacts expire after that). It drafts the release with a signed
   `unbound-release.json`, rebuilds the mods if the mod API major changed, and then publishes it as latest.

## Running the queue (maintainers)

- **Approve a new author:** add the label `approved` to their issue. To skip approval for them from then on,
  add their login to `.github/mod-signing/trusted-authors.txt`.
- **Block an author:** add their lowercase login to `kRevokedModAuthors` in
  `soh/soh/ModApi/ModTrust/RevokedMods.h`. The workflow refuses their requests at once, and their signed mods
  stop loading in the next release.
- **Revoke one package:** add its `content_hash` (from its `unbound-signature.json`) to `kRevokedModPackages`.
- A request whose run was cancelled stays queued; the workflow also runs every two hours and picks it up.
- **Store links:** `.github/mod-signing/store-links.json` lists the links the store's Links tab shows; they
  reach the catalog on the next signature.


## Pack presentation and issue commands

Authors can define packs in `packs.json` beside their mods. Each pack accepts `id`, `name`, `mods`,
`description` (up to 1000 characters), and `icon` (a game item texture such as `ITEM_BOW`,
`ITEM_SWORD_KOKIRI`, `ITEM_OCARINA_TIME`, or `ITEM_BOMB`). A mod's manifest also accepts `icon`.
Icons use images already available in Ship; unknown names display a default icon.

The issue author or a repository maintainer can comment on the open signing issue:

```text
/pack {"id":"starter","name":"Starter pack","description":"Recommended items","icon":"ITEM_BOW","mods":["whip","shield"]}
/mod-icon whip ITEM_SWORD_KOKIRI
/unpack starter
```

Put each command on its own line; one command is processed per comment. `/pack` creates or replaces
that pack using only mods published by this issue. There are at most 20 packs per issue. The issue
number namespaces the pack id. Issue changes take precedence over `packs.json` on later signatures;
`/unpack` also prevents the repository pack from reappearing. Recreate it with `/pack` when needed.
These commands update the signed catalog without rebuilding packages.

In the store, **Install pack** installs every member and its dependencies. **Configure pack**
lets players choose a subset, with Select all / Select none and per-mod status. Choices are saved between sessions. It selects installations;
it does not remove already installed mods or change their gameplay settings. Incompatible members
are unavailable for selection. Search matches pack names, authors and descriptions.


## Individual feature switches

A mod manifest can publish `"options": [{"id": "mask_blast", "name": "Blast Mask"}]` (up to 64).
The store shows these switches under **Configure pack > Feature options**, and in **Installed**.
Every option defaults to enabled. The store saves only `gMods.Store.<mod-id>.<option-id>`;
metadata cannot choose another CVar. The mod must read these settings during registration and
skip disabled features. Changes require a restart; existing item ownership is retained.

The NEI mods expose 17 wearable masks, four boss remains, two MM items, and five progressive
weapon chains independently. MM songs remain enabled with mm_collectables. Disabled weapon
chains leave vanilla weapon handling in place. Mods outside curated packs are grouped by catalog
owner (with author credits as a fallback). **Install pack** installs all members and dependencies
with one click; **Configure pack** selects members and individual features.


To run the native catalog regression from a Visual Studio Developer Command Prompt at the Unbound root:

```bat
cl /EHsc /std:c++20 /utf-8 /DFMT_HEADER_ONLY /Isoh /Ibuild/vcpkg/installed/x64-windows-static/include mod-sdk/tests/store_catalog_options.cpp soh/soh/ModApi/ModStore/StoreCatalog.cpp /Febuild/store-catalog-options.exe
build\store-catalog-options.exe
```

Resource packages can declare `"dependency_only": true`. They stay available to dependency resolution
and the Installed tab but are omitted from the automatic author groups. `mm_assets` uses this flag;
it is always included when installing a mod that declares it in `dependencies`.


Packs appear in a three-column grid. Click a card to expand its description and mod/feature options
below that row; the Install pack button remains directly available. Cards show a large pack icon,
a larger title (up to three lines), author, and pack version. Authors can add `"version": "1.0"` to
`packs.json` or `/pack` metadata (up to 64 UTF-8 bytes). Legacy packs without a version display
"Versions: per mod"; the expanded list shows the actual version of each member.
