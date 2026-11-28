# Fixed download links

- Players: https://github.com/skijer/Shipwright/releases/tag/nei-unbound-release
- Modders: https://github.com/skijer/Shipwright/releases/tag/nei-unbound-modders

Run **Actions > generate-builds > Run workflow** on `unbound-modapi`, leaving the tag empty. Once all three platforms and both editions build successfully, **update-downloads** replaces the ZIPs in these same two releases. The asset filenames stay fixed too.

`update-downloads` can also be run manually with a successful build's run ID to retry publication without recompiling. It requires the `unbound-download-source` artifact from this flow. Older runs cannot overwrite a newer published run. Tagged releases and pull requests do not update these pages.

The players' release is the repository's latest release and carries `unbound-release.json`, the signed manifest the in-game updater reads. Bump `PROJECT_FORK_VERSION` before a build that players should be offered. Publication does not change the mod catalog or the signing keys. Each page identifies the commit and build run of the current downloads.
