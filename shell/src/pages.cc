// Copyright (c) 2026 Open Cosmic Software. MIT licensed.

#include "pages.h"

namespace {

// Shared dark theme + small helper library used by every opennyx:// page.
const char kSharedCSS[] = R"CSS(
:root{
  --bg:#141419; --bg2:#18191e; --panel:#1c1d25; --panel2:#22232b;
  --border:#2a2c38; --border2:#34364a; --text:#e8e9f0; --dim:#9698a6;
  --dim2:#5c5f6a; --accent:#7a5cff; --accent2:#8d73ff; --danger:#ff5c72;
  --ok:#4ade80;
}
*{box-sizing:border-box}
html,body{margin:0;height:100%}
body{background:radial-gradient(1200px 700px at 50% -10%,#20222c 0%,
  #17181e 55%,#141419 100%);color:var(--text);
  font-family:'Segoe UI',system-ui,-apple-system,sans-serif;
  -webkit-font-smoothing:antialiased;min-height:100%}
a{color:var(--accent2);text-decoration:none}
a:hover{text-decoration:underline}
.top{display:flex;align-items:center;gap:16px;padding:22px 28px 8px;
  max-width:960px;margin:0 auto}
.brand{font-size:20px;font-weight:650;letter-spacing:.3px}
.brand .nyx{color:var(--accent)}
.nav{display:flex;gap:6px;margin-left:auto;flex-wrap:wrap}
.nav a{padding:7px 13px;border-radius:9px;color:var(--dim);font-size:13.5px;
  border:1px solid transparent}
.nav a:hover{background:var(--panel);color:var(--text);text-decoration:none}
.nav a.active{background:var(--panel2);color:var(--text);
  border-color:var(--border2)}
.wrap{max-width:960px;margin:0 auto;padding:14px 28px 60px}
h1{font-size:26px;font-weight:640;margin:14px 0 4px}
.sub{color:var(--dim);font-size:13.5px;margin:0 0 20px}
.card{background:var(--panel);border:1px solid var(--border);
  border-radius:14px;padding:20px 22px;margin:0 0 16px}
.card h2{font-size:15px;font-weight:600;margin:0 0 4px}
.card p.hint{color:var(--dim);font-size:12.5px;margin:2px 0 14px}
.row{display:flex;align-items:center;gap:14px;padding:11px 0;
  border-bottom:1px solid var(--border)}
.row:last-child{border-bottom:0}
.row .grow{flex:1;min-width:0}
.row .grow .t{font-size:14px;white-space:nowrap;overflow:hidden;
  text-overflow:ellipsis}
.row .grow .u{font-size:12px;color:var(--dim);white-space:nowrap;
  overflow:hidden;text-overflow:ellipsis}
.row .meta{color:var(--dim2);font-size:12px;white-space:nowrap}
input[type=text],input[type=search],input[type=url],input[type=password],
input.in,select{
  background:var(--bg2);color:var(--text);border:1px solid var(--border2);
  border-radius:9px;padding:9px 12px;font-size:14px;outline:none;
  font-family:inherit}
input:focus,select:focus{border-color:var(--accent)}
input[type=file]{color:var(--dim);font-size:13px}
.u{font-size:12.5px;color:var(--dim);line-height:1.5}
.pw{font-family:monospace;letter-spacing:1px}
.search{width:100%;margin:0 0 18px;padding:12px 15px;font-size:15px}
button,.btn{background:var(--accent);color:#fff;border:0;border-radius:9px;
  padding:9px 16px;font-size:13.5px;font-weight:600;cursor:pointer;
  font-family:inherit;transition:background .15s}
button:hover,.btn:hover{background:var(--accent2)}
button.ghost{background:var(--panel2);color:var(--text);
  border:1px solid var(--border2)}
button.ghost:hover{background:var(--border)}
button.danger{background:transparent;color:var(--danger);
  border:1px solid var(--danger)}
button.danger:hover{background:var(--danger);color:#fff}
button.mini{padding:5px 10px;font-size:12px;border-radius:7px}
.empty{color:var(--dim2);font-size:14px;text-align:center;padding:40px 0}
.field{display:flex;align-items:center;justify-content:space-between;
  gap:16px;padding:13px 0;border-bottom:1px solid var(--border)}
.field:last-child{border-bottom:0}
.field .lbl{font-size:14px}
.field .lbl small{display:block;color:var(--dim);font-size:12px;
  margin-top:2px;font-weight:400}
.switch{position:relative;width:44px;height:24px;flex:none}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;inset:0;background:var(--border2);border-radius:24px;
  transition:.2s;cursor:pointer}
.slider:before{content:"";position:absolute;height:18px;width:18px;left:3px;
  top:3px;background:#fff;border-radius:50%;transition:.2s}
.switch input:checked+.slider{background:var(--accent)}
.switch input:checked+.slider:before{transform:translateX(20px)}
.pill{display:inline-block;padding:3px 9px;border-radius:20px;font-size:11.5px;
  background:var(--panel2);color:var(--dim);border:1px solid var(--border2)}
.pill.ok{color:var(--ok);border-color:#2f5d43}
.pill.warn{color:#ffcf5c;border-color:#5d5330}
.bar{height:6px;background:var(--border);border-radius:6px;overflow:hidden;
  margin-top:6px}
.bar > i{display:block;height:100%;background:var(--accent);width:0}
.dlact{display:flex;gap:14px;margin-top:7px}
.linkbtn{background:none;border:none;color:var(--accent);cursor:pointer;
  padding:0;font-size:12.5px}
.linkbtn:hover{text-decoration:underline}
.toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);
  background:var(--panel2);border:1px solid var(--border2);color:var(--text);
  padding:11px 18px;border-radius:10px;font-size:13.5px;opacity:0;
  transition:opacity .2s;pointer-events:none;box-shadow:0 8px 30px rgba(0,0,0,.5)}
.toast.show{opacity:1}
.stat{display:flex;gap:26px;flex-wrap:wrap;margin:2px 0 4px}
.stat .n{font-size:26px;font-weight:680;color:var(--accent2)}
.stat .k{font-size:12px;color:var(--dim);margin-top:1px}
)CSS";

const char kSharedJS[] = R"JS(
const API='opennyx://api/';
async function api(path,opts){
  const r=await fetch(API+path,opts||{});
  const ct=r.headers.get('content-type')||'';
  if(ct.includes('json'))return r.json();
  return r.text();
}
function toast(msg){
  let t=document.querySelector('.toast');
  if(!t){t=document.createElement('div');t.className='toast';
    document.body.appendChild(t);}
  t.textContent=msg;t.classList.add('show');
  clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),1800);
}
function esc(s){return (s||'').replace(/[&<>"']/g,c=>(
  {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function fmtTime(ms){
  if(!ms)return '';
  const d=new Date(ms),now=new Date();
  const sameDay=d.toDateString()===now.toDateString();
  const t=d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});
  return sameDay?t:d.toLocaleDateString([],{month:'short',day:'numeric'})+' '+t;
}
function fmtBytes(b){
  if(!b||b<0)return '';
  const u=['B','KB','MB','GB'];let i=0;b=Number(b);
  while(b>=1024&&i<u.length-1){b/=1024;i++;}
  return b.toFixed(i?1:0)+' '+u[i];
}
function nav(sel){
  const p=location.href.split('/').pop().split('#')[0]||'newtab';
  document.querySelectorAll('.nav a').forEach(a=>{
    if(a.dataset.page===sel)a.classList.add('active');});
}
function go(url){ location.href = url; }
)JS";

std::string NavBar(const char* active) {
  std::string s =
      "<div class=\"top\"><div class=\"brand\">Open<span "
      "class=\"nyx\">Nyx</span></div><nav class=\"nav\">";
  struct Item {
    const char* page;
    const char* label;
  };
  const Item items[] = {{"newtab", "New Tab"},
                        {"history", "History"},
                        {"bookmarks", "Bookmarks"},
                        {"downloads", "Downloads"},
                        {"settings", "Settings"}};
  for (const auto& it : items) {
    const bool on = std::string(active) == it.page;
    s += "<a data-page=\"";
    s += it.page;
    s += "\" href=\"opennyx://";
    s += it.page;
    s += "\"";
    if (on) {
      s += " class=\"active\"";
    }
    s += ">";
    s += it.label;
    s += "</a>";
  }
  s += "</nav></div>";
  return s;
}

std::string Doc(const char* title, const char* active, const std::string& body,
                const std::string& script) {
  std::string s = "<!doctype html><html><head><meta charset=\"utf-8\">";
  s += "<title>";
  s += title;
  s += " — OpenNyx</title><style>";
  s += kSharedCSS;
  s += "</style></head><body>";
  s += NavBar(active);
  s += "<div class=\"wrap\">";
  s += body;
  s += "</div><script>";
  s += kSharedJS;
  s += script;
  s += "</script></body></html>";
  return s;
}

// ---- New tab ----
std::string NewTabPage() {
  const char* body = R"HTML(
<div style="max-width:640px;margin:8vh auto 0;text-align:center">
  <h1 style="font-size:52px;font-weight:650;margin:0 0 6px">
    Open<span style="color:var(--accent)">Nyx</span></h1>
  <p class="sub" style="margin:0 0 26px">Private by default</p>
  <form id="sf" style="display:flex;box-shadow:0 10px 40px rgba(0,0,0,.5);
    border-radius:14px;overflow:hidden;border:1px solid var(--border2)">
    <input id="q" type="search" autofocus autocomplete="off"
      placeholder="Search or enter address"
      style="flex:1;padding:16px 20px;font-size:16px;border:0;border-radius:0;
      background:var(--panel)">
    <button type="submit" style="border-radius:0;padding:16px 26px">Search</button>
  </form>
  <div id="tiles" style="display:flex;gap:12px;justify-content:center;
    margin-top:26px;flex-wrap:wrap"></div>
  <p class="sub" style="margin-top:30px;font-size:12.5px;color:var(--dim2)"
    id="foot">de-googled at runtime · tracker blocking on</p>
</div>
)HTML";
  const char* script = R"JS(
const tiles=[['🔍','Search','opennyx://newtab'],
  ['📚','Wikipedia','https://en.wikipedia.org/'],
  ['🐙','GitHub','https://github.com/'],
  ['📰','HN','https://news.ycombinator.com/'],
  ['🗺️','Maps','https://www.openstreetmap.org/'],
  ['🕘','History','opennyx://history'],
  ['⭐','Bookmarks','opennyx://bookmarks'],
  ['⚙️','Settings','opennyx://settings']];
document.getElementById('tiles').innerHTML=tiles.map(t=>
  `<a href="${t[2]}" style="display:flex;flex-direction:column;
   align-items:center;gap:7px;width:84px;padding:14px 8px;border-radius:12px;
   color:var(--dim);background:var(--panel);border:1px solid var(--border);
   font-size:12.5px"><span style="font-size:22px">${t[0]}</span>${t[1]}</a>`
  ).join('');
document.getElementById('sf').addEventListener('submit',e=>{
  e.preventDefault();
  const v=document.getElementById('q').value.trim();
  if(!v)return;
  // Let the shell resolve: navigate to a search/nav via the api resolve.
  api('resolve?q='+encodeURIComponent(v)).then(r=>{
    if(r&&r.url)go(r.url);
  });
});
api('config').then(c=>{
  const parts=[];
  parts.push('de-googled at runtime');
  parts.push(c.blocking_enabled?'tracker blocking on':'tracker blocking off');
  parts.push(c.doh_enabled?'DoH on':'DoH off');
  document.getElementById('foot').textContent=parts.join(' · ');
});
)JS";
  return Doc("New Tab", "newtab", body, script);
}

// ---- History ----
std::string HistoryPage() {
  const char* body = R"HTML(
<h1>History</h1>
<p class="sub">Your local browsing history. Stored on this device only.</p>
<input id="s" class="search" type="search" placeholder="Search history…"
  autocomplete="off">
<div style="display:flex;gap:10px;margin-bottom:16px">
  <button class="danger" id="clr">Clear all history</button>
</div>
<div class="card"><div id="list"><div class="empty">Loading…</div></div></div>
)HTML";
  const char* script = R"JS(
let timer;
async function load(){
  const q=document.getElementById('s').value.trim();
  const r=await api('history?q='+encodeURIComponent(q)+'&limit=500');
  const list=document.getElementById('list');
  if(!r.items||!r.items.length){
    list.innerHTML='<div class="empty">No history yet.</div>';return;}
  list.innerHTML=r.items.map(h=>
    `<div class="row"><div class="grow">
      <div class="t"><a href="${esc(h.url)}">${esc(h.title||h.url)}</a></div>
      <div class="u">${esc(h.url)}</div></div>
      <div class="meta">${fmtTime(h.ts)}</div></div>`).join('');
}
document.getElementById('s').addEventListener('input',()=>{
  clearTimeout(timer);timer=setTimeout(load,150);});
document.getElementById('clr').addEventListener('click',async()=>{
  await api('history/clear',{method:'POST'});
  toast('History cleared');load();});
load();
)JS";
  return Doc("History", "history", body, script);
}

// ---- Bookmarks ----
std::string BookmarksPage() {
  const char* body = R"HTML(
<h1>Bookmarks</h1>
<p class="sub">Saved pages. Click the ☆ in the toolbar to add the current page.</p>
<div class="card"><div id="list"><div class="empty">Loading…</div></div></div>
)HTML";
  const char* script = R"JS(
async function load(){
  const r=await api('bookmarks');
  const list=document.getElementById('list');
  if(!r.items||!r.items.length){
    list.innerHTML='<div class="empty">No bookmarks yet.</div>';return;}
  list.innerHTML=r.items.map(b=>
    `<div class="row"><div class="grow">
      <div class="t"><a href="${esc(b.url)}">${esc(b.title||b.url)}</a></div>
      <div class="u">${esc(b.url)}</div></div>
      <button class="ghost mini" data-u="${esc(b.url)}">Remove</button></div>`
    ).join('');
  list.querySelectorAll('button[data-u]').forEach(btn=>{
    btn.addEventListener('click',async()=>{
      await api('bookmarks/remove',{method:'POST',
        headers:{'content-type':'application/json'},
        body:JSON.stringify({url:btn.dataset.u})});
      toast('Removed');load();});});
}
load();
)JS";
  return Doc("Bookmarks", "bookmarks", body, script);
}

// ---- Downloads ----
std::string DownloadsPage() {
  const char* body = R"HTML(
<h1>Downloads</h1>
<p class="sub">Files downloaded with OpenNyx.</p>
<div style="display:flex;gap:10px;margin-bottom:16px">
  <button class="ghost" id="openfolder">Open downloads folder</button>
  <button class="ghost" id="clr">Clear list</button>
</div>
<div class="card"><div id="list"><div class="empty">Loading…</div></div></div>
)HTML";
  const char* script = R"JS(
async function load(){
  const r=await api('downloads');
  const list=document.getElementById('list');
  if(!r.items||!r.items.length){
    list.innerHTML='<div class="empty">No downloads yet.</div>';return;}
  list.innerHTML=r.items.map(d=>{
    let status,pill;
    if(d.canceled){status='Canceled';pill='<span class="pill warn">canceled</span>';}
    else if(d.complete){status=fmtBytes(d.total_bytes);pill='<span class="pill ok">done</span>';}
    else{status=d.percent+'% · '+fmtBytes(d.received_bytes);
      pill='<span class="pill">downloading</span>';}
    const barhtml=d.complete||d.canceled?'':
      `<div class="bar"><i style="width:${d.percent}%"></i></div>`;
    // Only completed, non-canceled files can be opened / revealed.
    const p=(d.full_path||'').replace(/"/g,'&quot;');
    const actions=(d.complete&&!d.canceled&&p)?
      `<div class="dlact">`+
      `<button class="linkbtn" data-open="${p}">Open</button>`+
      `<button class="linkbtn" data-reveal="${p}">Show in folder</button>`+
      `</div>`:'';
    return `<div class="row"><div class="grow">
      <div class="t">${esc(d.filename)} ${pill}</div>
      <div class="u">${esc(d.url)}</div>${barhtml}${actions}</div>
      <div class="meta">${esc(status)}</div></div>`;}).join('');
  // Wire the per-file Open / Show-in-folder buttons.
  list.querySelectorAll('[data-open]').forEach(b=>b.addEventListener('click',
    async()=>{const r=await api('downloads/open',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({path:b.getAttribute('data-open')})});
      if(!r||!r.ok)toast('Could not open file');}));
  list.querySelectorAll('[data-reveal]').forEach(b=>b.addEventListener('click',
    async()=>{const r=await api('downloads/reveal',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({path:b.getAttribute('data-reveal')})});
      if(!r||!r.ok)toast('Could not show file');}));
}
document.getElementById('clr').addEventListener('click',async()=>{
  await api('downloads/clear',{method:'POST'});toast('Cleared');load();});
document.getElementById('openfolder').addEventListener('click',async()=>{
  const r=await api('downloads/openfolder',{method:'POST'});
  if(!r||!r.ok)toast('Could not open folder');});
load();
setInterval(load,1200);
)JS";
  return Doc("Downloads", "downloads", body, script);
}

// ---- Passwords ----
std::string PasswordsPage() {
  const char* body = R"HTML(
<h1>Passwords</h1>
<p class="sub">Your saved logins, encrypted at rest with Windows DPAPI
(tied to your Windows account \xE2\x80\x94 the same protection Chrome and Opera use).</p>

<div class="card" style="margin-bottom:16px">
  <div style="font-weight:600;margin-bottom:6px">Import from another browser</div>
  <div class="u" style="margin-bottom:10px">
    In Chrome / Opera / Edge / Brave open
    <b>Settings \xE2\x86\x92 Passwords \xE2\x86\x92 \xE2\x8B\xAF \xE2\x86\x92 Export passwords</b>,
    save the CSV, then pick it here. In Firefox use
    <b>about:logins \xE2\x86\x92 \xE2\x8B\xAF \xE2\x86\x92 Export Logins</b>.
  </div>
  <input type="file" id="csv" accept=".csv,text/csv" />
  <button class="primary" id="importbtn" style="margin-left:8px">Import CSV</button>
  <div class="u" id="impstatus" style="margin-top:8px"></div>
</div>

<div class="card" style="margin-bottom:16px">
  <div style="font-weight:600;margin-bottom:8px">Add a login manually</div>
  <div style="display:flex;gap:8px;flex-wrap:wrap">
    <input class="in" id="a_origin" placeholder="https://example.com" style="flex:1;min-width:180px" />
    <input class="in" id="a_user" placeholder="username or email" style="flex:1;min-width:140px" />
    <input class="in" id="a_pass" type="password" placeholder="password" style="flex:1;min-width:140px" />
    <button class="ghost" id="addbtn">Save</button>
  </div>
</div>

<div style="display:flex;gap:10px;margin-bottom:14px">
  <input class="in" id="filter" placeholder="Search logins\xE2\x80\xA6" style="flex:1" />
  <button class="ghost" id="clr">Clear all</button>
</div>
<div class="card"><div id="list"><div class="empty">Loading\xE2\x80\xA6</div></div></div>
)HTML";
  const char* script = R"JS(
let ALL=[];
function esc2(s){return (s||'').replace(/"/g,'&quot;');}
async function load(){
  const r=await api('passwords');
  ALL=r.items||[];
  render();
}
function render(){
  const q=(document.getElementById('filter').value||'').toLowerCase();
  const list=document.getElementById('list');
  const items=ALL.filter(p=>!q||(p.origin+' '+p.username).toLowerCase().includes(q));
  if(!items.length){list.innerHTML='<div class="empty">No saved logins.</div>';return;}
  list.innerHTML=items.map((p,i)=>
    `<div class="row"><div class="grow">
      <div class="t">${esc(p.origin)}</div>
      <div class="u">${esc(p.username)}</div>
      <div class="u"><span class="pw" data-i="${i}">\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2</span>
        <button class="linkbtn" data-reveal="${i}">Show</button>
        <button class="linkbtn" data-copy="${i}">Copy</button></div></div>
      <button class="ghost mini" data-del-o="${esc2(p.origin)}" data-del-u="${esc2(p.username)}">Remove</button></div>`
    ).join('');
  list.querySelectorAll('[data-reveal]').forEach(b=>b.addEventListener('click',()=>{
    const i=+b.getAttribute('data-reveal');const span=list.querySelector('.pw[data-i="'+i+'"]');
    if(span.dataset.shown){span.textContent='\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2';delete span.dataset.shown;b.textContent='Show';}
    else{span.textContent=items[i].password;span.dataset.shown='1';b.textContent='Hide';}}));
  list.querySelectorAll('[data-copy]').forEach(b=>b.addEventListener('click',()=>{
    const i=+b.getAttribute('data-copy');navigator.clipboard&&navigator.clipboard.writeText(items[i].password);toast('Copied');}));
  list.querySelectorAll('[data-del-o]').forEach(b=>b.addEventListener('click',async()=>{
    await api('passwords/remove',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({origin:b.getAttribute('data-del-o'),username:b.getAttribute('data-del-u')})});
    toast('Removed');load();}));
}
document.getElementById('filter').addEventListener('input',render);
document.getElementById('addbtn').addEventListener('click',async()=>{
  const o=document.getElementById('a_origin').value.trim();
  const u=document.getElementById('a_user').value.trim();
  const p=document.getElementById('a_pass').value;
  if(!o&&!u){toast('Enter a site + username');return;}
  await api('passwords/add',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({origin:o,username:u,password:p})});
  document.getElementById('a_origin').value='';document.getElementById('a_user').value='';
  document.getElementById('a_pass').value='';toast('Saved');load();});
document.getElementById('clr').addEventListener('click',async()=>{
  if(!confirm('Delete ALL saved logins?'))return;
  await api('passwords/clear',{method:'POST'});toast('Cleared');load();});

// --- CSV import ---
// Robust-enough CSV line splitter that respects double-quoted fields.
function parseCSV(text){
  const rows=[];let row=[];let cur='';let q=false;
  for(let i=0;i<text.length;i++){const c=text[i];
    if(q){if(c==='"'){if(text[i+1]==='"'){cur+='"';i++;}else q=false;}else cur+=c;}
    else{if(c==='"')q=true;
      else if(c===','){row.push(cur);cur='';}
      else if(c==='\n'){row.push(cur);rows.push(row);row=[];cur='';}
      else if(c==='\r'){}
      else cur+=c;}}
  if(cur.length||row.length){row.push(cur);rows.push(row);}
  return rows;
}
document.getElementById('importbtn').addEventListener('click',async()=>{
  const f=document.getElementById('csv').files[0];
  const st=document.getElementById('impstatus');
  if(!f){st.textContent='Pick a CSV file first.';return;}
  st.textContent='Reading\xE2\x80\xA6';
  const text=await f.text();
  const rows=parseCSV(text);
  if(rows.length<2){st.textContent='CSV looks empty.';return;}
  const head=rows[0].map(h=>h.trim().toLowerCase());
  // Chrome/Opera/Edge/Brave: name,url,username,password[,note]
  // Firefox: url,username,password,... ("url" or "hostname").
  const iUrl=head.findIndex(h=>h==='url'||h==='origin'||h==='hostname'||h==='login_uri');
  const iUser=head.findIndex(h=>h==='username'||h==='login_username'||h==='user');
  const iPass=head.findIndex(h=>h==='password'||h==='login_password'||h==='pass');
  if(iUser<0||iPass<0){st.textContent='Could not find username/password columns in this CSV.';return;}
  const items=[];
  for(let r=1;r<rows.length;r++){const row=rows[r];if(!row.length||(row.length===1&&!row[0]))continue;
    items.push({origin:iUrl>=0?(row[iUrl]||''):'',username:row[iUser]||'',password:row[iPass]||''});}
  const res=await api('passwords/import',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({items})});
  st.textContent='Imported '+((res&&res.count)||0)+' logins.';
  toast('Imported '+((res&&res.count)||0));load();});
load();
)JS";
  return Doc("Passwords", "passwords", body, script);
}

// ---- Settings ----
std::string SettingsPage() {
  const char* body = R"HTML(
<h1>Settings</h1>
<p class="sub">Preferences are stored in a local config file on this device.</p>

<div class="card">
  <h2>Search &amp; startup</h2>
  <p class="hint">Choose your default search engine and homepage.</p>
  <div class="field"><div class="lbl">Default search engine
    <small>Used by the address bar and new-tab box</small></div>
    <select id="engine">
      <option value="brave">Brave Search</option>
      <option value="duckduckgo">DuckDuckGo</option>
      <option value="mojeek">Mojeek</option>
      <option value="custom">Custom…</option>
    </select></div>
  <div class="field" id="customRow" style="display:none">
    <div class="lbl">Custom search URL
      <small>Use {q} where the query goes, e.g. https://searx.be/search?q={q}</small></div>
    <input id="customUrl" type="url" style="width:340px"
      placeholder="https://example.com/search?q={q}"></div>
  <div class="field"><div class="lbl">Homepage
    <small>Where the ⌂ button and new windows start</small></div>
    <input id="homepage" type="url" style="width:340px"
      placeholder="opennyx://newtab"></div>
</div>

<div class="card">
  <h2>Privacy &amp; security</h2>
  <p class="hint">OpenNyx is de-googled at runtime: no Google API keys, metrics
    and telemetry disabled, no background phone-home.</p>
  <div class="field"><div class="lbl">Block trackers &amp; ads
    <small id="blkInfo">Bundled blocklist</small></div>
    <label class="switch"><input type="checkbox" id="blocking">
      <span class="slider"></span></label></div>
  <div class="field"><div class="lbl">DNS-over-HTTPS (DoH)
    <small>Encrypts DNS lookups. Takes full effect after restart.</small></div>
    <label class="switch"><input type="checkbox" id="doh">
      <span class="slider"></span></label></div>
  <div class="field"><div class="lbl">DoH resolver
    <small>Non-US options first. Applied on restart.</small></div>
    <select id="resolver">
      <option value="quad9">Quad9 (Switzerland)</option>
      <option value="dns0">DNS0.eu (Europe)</option>
      <option value="mullvad">Mullvad (Sweden)</option>
      <option value="cloudflare">Cloudflare (US, opt-in)</option>
      <option value="custom">Custom template…</option>
    </select></div>
  <div class="field" id="dohCustomRow" style="display:none">
    <div class="lbl">Custom DoH template
      <small>RFC 8484 URI template, e.g. https://dns.example/dns-query{?dns}</small></div>
    <input id="dohCustom" type="url" style="width:340px"
      placeholder="https://dns.example/dns-query{?dns}"></div>
</div>

<div class="card">
  <h2>Tracker blocking</h2>
  <div class="stat">
    <div><div class="n" id="statList">–</div><div class="k">domains in blocklist</div></div>
    <div><div class="n" id="statTotal">–</div><div class="k">requests blocked this session</div></div>
  </div>
</div>

<div class="card">
  <h2>Clear browsing data</h2>
  <p class="hint">Removes local history and the downloads list. Bookmarks and
    settings are kept.</p>
  <button class="danger" id="clearData">Clear browsing data</button>
</div>

<div style="display:flex;gap:10px;margin-top:4px">
  <button id="save">Save settings</button>
  <span id="savedMsg" class="pill ok" style="display:none;align-self:center">Saved</span>
</div>
)HTML";
  const char* script = R"JS(
let cfg={};
function fill(){
  document.getElementById('engine').value=cfg.search_engine||'brave';
  document.getElementById('customUrl').value=cfg.custom_search_url||'';
  document.getElementById('homepage').value=cfg.homepage||'opennyx://newtab';
  document.getElementById('blocking').checked=!!cfg.blocking_enabled;
  document.getElementById('doh').checked=!!cfg.doh_enabled;
  document.getElementById('resolver').value=cfg.doh_resolver||'quad9';
  document.getElementById('dohCustom').value=cfg.doh_custom_template||'';
  toggleCustom();
}
function toggleCustom(){
  document.getElementById('customRow').style.display=
    document.getElementById('engine').value==='custom'?'':'none';
  document.getElementById('dohCustomRow').style.display=
    document.getElementById('resolver').value==='custom'?'':'none';
}
document.getElementById('engine').addEventListener('change',toggleCustom);
document.getElementById('resolver').addEventListener('change',toggleCustom);
async function save(){
  cfg.search_engine=document.getElementById('engine').value;
  cfg.custom_search_url=document.getElementById('customUrl').value.trim();
  cfg.homepage=document.getElementById('homepage').value.trim()||'opennyx://newtab';
  cfg.blocking_enabled=document.getElementById('blocking').checked;
  cfg.doh_enabled=document.getElementById('doh').checked;
  cfg.doh_resolver=document.getElementById('resolver').value;
  cfg.doh_custom_template=document.getElementById('dohCustom').value.trim();
  await api('config',{method:'POST',headers:{'content-type':'application/json'},
    body:JSON.stringify(cfg)});
  const m=document.getElementById('savedMsg');
  m.style.display='inline-block';setTimeout(()=>m.style.display='none',1500);
  toast('Settings saved');
}
document.getElementById('save').addEventListener('click',save);
document.getElementById('clearData').addEventListener('click',async()=>{
  await api('cleardata',{method:'POST'});toast('Browsing data cleared');});
async function load(){
  cfg=await api('config');fill();
  const st=await api('blockstats');
  document.getElementById('statList').textContent=st.list_size;
  document.getElementById('statTotal').textContent=st.total_blocked;
  document.getElementById('blkInfo').textContent=
    'Bundled blocklist · '+st.list_size+' domains';
}
load();
)JS";
  return Doc("Settings", "settings", body, script);
}

}  // namespace

std::string GetSharedChrome() {
  return std::string(kSharedCSS) + kSharedJS;
}

// ---- About ----
std::string AboutPage() {
  // NOTE: raw string literals do NOT process \x escapes, so emoji/UTF-8 are
  // written as literal bytes here (the file is UTF-8).
  const char* body = R"HTML(
<div style="text-align:center;padding:40px 0 10px">
  <div style="font-size:64px;line-height:1">🦞</div>
  <h1 style="margin:16px 0 4px">OpenNyx</h1>
  <p class="sub">A privacy-first, de-googled browser for developers.</p>
</div>
<div class="card">
  <div class="row"><div class="grow"><div class="t">Engine</div>
    <div class="u">Chromium via CEF (de-googled at runtime)</div></div></div>
  <div class="row"><div class="grow"><div class="t">Default search</div>
    <div class="u">Brave Search</div></div></div>
  <div class="row"><div class="grow"><div class="t">Secure DNS</div>
    <div class="u">DNS-over-HTTPS via Quad9 (Swiss non-profit)</div></div></div>
  <div class="row"><div class="grow"><div class="t">License</div>
    <div class="u">MIT · Open Cosmic Software</div></div></div>
</div>
<p class="sub" style="text-align:center;margin-top:20px">
  Made with 🦞 by Nyx ·
  <a href="https://github.com/Open-Cosmic-Software/OpenNyx">github.com/Open-Cosmic-Software/OpenNyx</a>
</p>
)HTML";
  const char* script = "";
  return Doc("About", "about", body, script);
}

std::string GetOpenNyxPageHTML(const std::string& page) {
  if (page == "history") {
    return HistoryPage();
  }
  if (page == "bookmarks") {
    return BookmarksPage();
  }
  if (page == "downloads") {
    return DownloadsPage();
  }
  if (page == "settings") {
    return SettingsPage();
  }
  if (page == "passwords") {
    return PasswordsPage();
  }
  if (page == "about") {
    return AboutPage();
  }
  return NewTabPage();
}
