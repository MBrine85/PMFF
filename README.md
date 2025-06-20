# PMFF - Package Manager For Frogs

PMFF is a simple package manager for Windows designed to download, install, list, and remove applications using manifests.

## Features

- Install apps via manifests stored locally.
- List installed apps.
- Remove installed apps.
- Supports `.zip` archives for installation.
- Does **not** currently support `.exe` installers.
- Uses Windows HTTP API for downloading.

## Requirements

- Windows OS
- C++17 compatible compiler
- PowerShell (for unzipping `.zip` archives)

## Installation

Build using g++ with required libraries:

```sh
g++ main.cpp -static -static-libgcc -static-libstdc++ -lwinhttp -o pmff.exe
```

## Usage

```sh
pmff list
pmff path
pmff install <appname>
pmff remove <appname>
pmff create <url>
pmff help
```

## App Manifests

Manifests are JSON files stored under `app_manifests` directory inside the PMFF root folder. They describe the app's metadata and download URL.

Example manifest `app_manifests/git.json`:

```json
{
  "name": "app",
  "version": "1.2.3",
  "url": "https://download/url/app.zip"
}
```

## License

PMFF is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.

## Disclaimer

This is a simple experimental package manager. Use at your own risk.
