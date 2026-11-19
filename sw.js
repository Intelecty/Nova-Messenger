/* Nova Messenger — Service Worker
   • оболочка приложения (index.html, манифест, иконки) кэшируется при установке
   • страницы: сначала сеть (свежая версия), при отсутствии связи — кэш
   • статика и CDN (шрифты, библиотеки): отдаём из кэша и обновляем в фоне
   • API (Supabase, Firebase, перевод и т.п.) и звонки в кэш НЕ попадают
   Чтобы выкатить обновление, поменяйте VERSION. */
const VERSION = "nova-v1";
const SHELL_CACHE = VERSION + "-shell";
const RUNTIME_CACHE = VERSION + "-runtime";
const RUNTIME_LIMIT = 120;

const SHELL = [
  "./",
  "index.html",
  "manifest.json",
  "icons/icon-192.png",
  "icons/icon-512.png",
  "icons/icon-maskable-192.png",
  "icons/icon-maskable-512.png",
  "icons/apple-touch-icon.png"
];

const CDN_HOSTS = new Set(["cdn.jsdelivr.net", "unpkg.com", "fonts.googleapis.com", "fonts.gstatic.com"]);
const API_RE = /supabase\.co|firestore\.googleapis|firebaseio|identitytoolkit|securetoken|translate\.googleapis|\/realtime\/|\/rest\/v1\/|\/auth\/v1\/|\/storage\/v1\//;

const isCacheableCross = (u) =>
  CDN_HOSTS.has(u.hostname) || (u.hostname === "www.gstatic.com" && u.pathname.startsWith("/firebasejs/"));

self.addEventListener("install", (e) => {
  e.waitUntil((async () => {
    const cache = await caches.open(SHELL_CACHE);
    // не валим установку, если какого-то файла нет
    await Promise.all(SHELL.map((u) => cache.add(new Request(u, { cache: "reload" })).catch(() => 0)));
    await self.skipWaiting();
  })());
});

self.addEventListener("activate", (e) => {
  e.waitUntil((async () => {
    const keep = new Set([SHELL_CACHE, RUNTIME_CACHE]);
    for (const k of await caches.keys()) {
      if (k.startsWith("nova-") && !keep.has(k)) await caches.delete(k);
    }
    await self.clients.claim();
  })());
});

self.addEventListener("message", (e) => {
  if (e.data === "SKIP_WAITING" || (e.data && e.data.type === "SKIP_WAITING")) self.skipWaiting();
});

self.addEventListener("fetch", (e) => {
  const req = e.request;
  if (req.method !== "GET" || req.headers.has("range")) return;
  let url;
  try { url = new URL(req.url); } catch (_) { return; }
  if (url.protocol !== "http:" && url.protocol !== "https:") return;
  if (API_RE.test(req.url)) return;                       // API — всегда напрямую в сеть

  if (url.origin === self.location.origin) {
    if (req.mode === "navigate") e.respondWith(navigate(req));
    else e.respondWith(staleWhileRevalidate(e, SHELL_CACHE));
  } else if (isCacheableCross(url)) {
    e.respondWith(staleWhileRevalidate(e, RUNTIME_CACHE));
  }
  // остальные сторонние запросы не трогаем
});

function withTimeout(p, ms) {
  return new Promise((resolve, reject) => {
    const t = setTimeout(() => reject(new Error("timeout")), ms);
    p.then((v) => { clearTimeout(t); resolve(v); }, (err) => { clearTimeout(t); reject(err); });
  });
}

async function navigate(req) {
  const cache = await caches.open(SHELL_CACHE);
  const u = new URL(req.url); u.search = ""; u.hash = "";
  const key = u.href;
  try {
    const res = await withTimeout(fetch(req), 4000);
    if (res && res.ok) cache.put(key, res.clone());
    return res;
  } catch (_) {
    return (await cache.match(key)) ||
           (await cache.match("index.html")) ||
           (await cache.match("./")) ||
           offlinePage();
  }
}

async function staleWhileRevalidate(e, cacheName) {
  const req = e.request;
  const cache = await caches.open(cacheName);
  const hit = await cache.match(req);
  const net = fetch(req).then((res) => {
    if (res && (res.ok || res.type === "opaque")) {
      cache.put(req, res.clone()).then(() => trim(cache)).catch(() => 0);
    }
    return res;
  }).catch(() => null);
  if (hit) { e.waitUntil(net); return hit; }
  const res = await net;
  return res || new Response("", { status: 504, statusText: "Offline" });
}

async function trim(cache) {
  if (cache === undefined) return;
  const keys = await cache.keys();
  if (keys.length > RUNTIME_LIMIT) {
    for (const k of keys.slice(0, keys.length - RUNTIME_LIMIT)) await cache.delete(k);
  }
}

function offlinePage() {
  const html = '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>Nova — нет связи</title><body style="margin:0;min-height:100vh;display:grid;place-items:center;background:#0e1118;color:#eceff6;font:16px system-ui,sans-serif;text-align:center">' +
    '<div><div style="font-size:56px">📡</div><h2>Нет подключения</h2><p style="color:#8e97ab">Откройте Nova один раз с интернетом, чтобы она сохранилась для работы офлайн.</p>' +
    '<button onclick="location.reload()" style="padding:12px 22px;border:0;border-radius:12px;background:#6c5ce7;color:#fff;font:inherit">Повторить</button></div>';
  return new Response(html, { status: 503, headers: { "Content-Type": "text/html; charset=utf-8" } });
}
