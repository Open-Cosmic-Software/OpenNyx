// OpenNyx shell logic (M1 scaffold)
//
// This is the UI-side controller. It is intentionally engine-agnostic: it emits
// intents ("navigate", "new tab", "close tab", "back") and, on a real Tauri build,
// forwards them to the Rust backend which drives the actual webviews. Until the
// Rust side is wired up (see ../README.md), it runs as a UI-only mock so the shell
// is clickable in any browser during development.

const DEFAULT_SEARCH = "https://duckduckgo.com/?q="; // privacy engine, never Google

// Try to reach the Tauri bridge; fall back to a no-op mock for pure-web dev.
let invoke = async (cmd, args) => {
  console.info(`[mock] invoke ${cmd}`, args || {});
};
try {
  const api = await import("@tauri-apps/api/core");
  invoke = api.invoke;
} catch {
  /* running outside Tauri — mock stays */
}

const state = { tabs: [], activeId: null, nextId: 1 };

const els = {
  tabs: document.getElementById("tabs"),
  newTab: document.getElementById("new-tab"),
  omnibox: document.getElementById("omnibox"),
  back: document.getElementById("back"),
  forward: document.getElementById("forward"),
  reload: document.getElementById("reload"),
};

/** Turn omnibox text into a URL: real URL → use it; otherwise → search. */
function toUrl(input) {
  const t = input.trim();
  if (!t) return null;
  if (/^https?:\/\//i.test(t)) return t;
  // looks like a domain (has a dot, no spaces)?
  if (/^[^\s]+\.[^\s]+$/.test(t)) return `https://${t}`;
  return DEFAULT_SEARCH + encodeURIComponent(t);
}

function newTab(url) {
  const id = state.nextId++;
  state.tabs.push({ id, title: "New tab", url: url || null });
  setActive(id);
  render();
  invoke("open_tab", { id, url });
}

function closeTab(id) {
  const idx = state.tabs.findIndex((t) => t.id === id);
  if (idx === -1) return;
  state.tabs.splice(idx, 1);
  invoke("close_tab", { id });
  if (state.activeId === id) {
    const next = state.tabs[idx] || state.tabs[idx - 1];
    setActive(next ? next.id : null);
  }
  render();
}

function setActive(id) {
  state.activeId = id;
  const tab = state.tabs.find((t) => t.id === id);
  els.omnibox.value = tab && tab.url ? tab.url : "";
  if (id != null) invoke("activate_tab", { id });
}

function navigate(input) {
  const url = toUrl(input);
  if (!url) return;
  let tab = state.tabs.find((t) => t.id === state.activeId);
  if (!tab) return newTab(url);
  tab.url = url;
  tab.title = url.replace(/^https?:\/\//, "").split("/")[0];
  invoke("navigate", { id: tab.id, url });
  render();
}

function render() {
  els.tabs.innerHTML = "";
  for (const tab of state.tabs) {
    const el = document.createElement("div");
    el.className = "tab" + (tab.id === state.activeId ? " active" : "");
    el.setAttribute("role", "tab");
    el.innerHTML = `<span class="title"></span><button class="close" title="Close">×</button>`;
    el.querySelector(".title").textContent = tab.title;
    el.addEventListener("click", (e) => {
      if (e.target.classList.contains("close")) return;
      setActive(tab.id);
      render();
    });
    el.querySelector(".close").addEventListener("click", (e) => {
      e.stopPropagation();
      closeTab(tab.id);
    });
    els.tabs.appendChild(el);
  }
}

// Wire up controls
els.newTab.addEventListener("click", () => newTab());
els.omnibox.addEventListener("keydown", (e) => {
  if (e.key === "Enter") navigate(els.omnibox.value);
});
els.back.addEventListener("click", () => invoke("go_back", { id: state.activeId }));
els.forward.addEventListener("click", () => invoke("go_forward", { id: state.activeId }));
els.reload.addEventListener("click", () => invoke("reload", { id: state.activeId }));

// Keyboard: Ctrl+T new tab, Ctrl+W close tab, Ctrl+L focus omnibox
window.addEventListener("keydown", (e) => {
  if (!(e.ctrlKey || e.metaKey)) return;
  if (e.key === "t") { e.preventDefault(); newTab(); }
  else if (e.key === "w") { e.preventDefault(); if (state.activeId) closeTab(state.activeId); }
  else if (e.key === "l") { e.preventDefault(); els.omnibox.select(); }
});

// Start with one tab
newTab();
