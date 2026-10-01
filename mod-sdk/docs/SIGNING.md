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
   mods folder.
3. The workflow answers the issue: either a release with your signed `.o2r` files, or the step that failed.

Requests are signed one at a time, oldest first. Your first request waits until a maintainer approves it
(label `approved`); once you are on the trusted list, later ones go straight to the queue.

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
| `files` | `PickUserFile`, `SaveToModsFolder`, `ExtractRomToModsFolder`, and the older `ExportArchiveFiles`, `ExtractRom` | Asked every time: a file is only read once the player picks it, and nothing is written without a yes. Writes only land in `mods/` |

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
| Layout | Build scripts, executables, symbolic links, files other than sources, docs and `manifest.json` outside `assets/` |
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

## Running the queue (maintainers)

- **Approve a new author:** add the label `approved` to their issue. To skip approval for them from then on,
  add their login to `.github/mod-signing/trusted-authors.txt`.
- **Block an author:** add their lowercase login to `kRevokedModAuthors` in
  `soh/soh/ModApi/ModTrust/RevokedMods.h`. The workflow refuses their requests at once, and their signed mods
  stop loading in the next release.
- **Revoke one package:** add its `content_hash` (from its `unbound-signature.json`) to `kRevokedModPackages`.
- A request whose run was cancelled stays queued; the workflow also runs every two hours and picks it up.
