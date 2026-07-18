# OpenNyx Shell (M1)

The **shell** is OpenNyx's UI layer: window chrome, tab strip, omnibox, command
palette, themes. For M1 it is a **Tauri v2** app — a Rust backend (Wry → WebView2 on
Windows) with a plain static web frontend. See `../docs/M1-PLAN.md` for the full
rationale and the engine decision.

> M1 status: **scaffold only.** Dependencies are *not* vendored here to keep the repo
> small. Run the init steps below to materialize `src-tauri/` and `node_modules/`.

## Layout

```
shell/
├── README.md            ← you are here
├── package.json         ← frontend + tauri-cli scripts
├── index.html           ← the shell UI (tab strip + omnibox) — static, no framework yet
├── src/
│   ├── main.js          ← shell logic (tabs, omnibox → navigate)
│   └── styles.css       ← dark-first theme
└── src-tauri/           ← generated on first init (NOT committed until M1 build)
    ├── Cargo.toml
    ├── tauri.conf.json
    └── src/main.rs
```

## First-time setup (materialize the Tauri backend)

Run **on a machine with Rust + Node** (or let CI do it — the frontend is enough to
commit). This pulls the Tauri CLI and generates `src-tauri/`:

```bash
cd shell
npm install                     # installs @tauri-apps/cli, @tauri-apps/api
npm create tauri-app@latest -- --manifest-path .   # or: npx tauri init
```

When `tauri init` asks:
- **frontend dev command:** *(leave empty — static files)*
- **frontend dist dir:** `../` (this folder; `index.html` lives here)
- **frontend dev server url:** *(leave empty)*

Then enable multi-webview (needed for real tabs) in `src-tauri/Cargo.toml`:

```toml
[dependencies]
tauri = { version = "2", features = ["unstable"] }
```

## Build (Windows, via CI)

We do **not** build Windows locally on the Linux server. Push to `main` and let
`.github/workflows/build-windows.yml` (see M1-PLAN §5) build on a `windows-latest`
runner and upload the `.exe` as an artifact.

Local Linux smoke test (renders via WebKitGTK, just to see the UI):

```bash
cd shell && npm run tauri dev
```

## M1 acceptance

- Window opens with a tab strip + omnibox.
- Open ≥2 tabs, type a URL, page loads in the active tab, close a tab.
- Default search = a privacy engine (never Google).
- A downloadable Windows `.exe` artifact from CI that does the above.
