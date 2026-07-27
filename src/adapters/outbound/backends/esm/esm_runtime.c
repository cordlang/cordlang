/*
 * The client runtime, embedded in the executable and served at
 * /@cord/runtime.js. No npm, no bundler, no build step.
 *
 * Model: components are plain functions `(props, $) => vnode`. State lives on a
 * per-instance store keyed by declaration name, so re-running the function is
 * idempotent. DOM updates go through a keyed vnode diff.
 *
 * A component or fragment can occupy several sibling DOM nodes, so a node's DOM
 * range is DERIVED on demand (domsOf) instead of cached on the vnode. Caching it
 * is where hand-written runtimes usually rot: an ancestor keeps a stale node
 * list after a descendant swaps its subtree. Empty fragments and null-rendering
 * components keep a comment placeholder, so every vnode always owns >= 1 node
 * and insertion points stay computable.
 *
 * JS below intentionally avoids double quotes and backslashes so it survives as
 * a C string literal without escaping.
 */
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/html_escape.h"
#include "domain/diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *RUNTIME_JS =
    "/* Cordlang native ESM runtime */\n"
    "const FRAG = Symbol('cord.frag');\n"
    "export const Frag = FRAG;\n"
    "const isFn = (v) => typeof v === 'function';\n"
    "\n"
    "/* ── vnodes ─────────────────────────────────────────── */\n"
    "\n"
    "function mkText(value) {\n"
    "  return { type: '#text', props: {}, key: undefined, kids: [], text: String(value),\n"
    "           dom: null, hole: null, child: null, inst: null };\n"
    "}\n"
    "\n"
    "function flatten(input, out) {\n"
    "  if (input === null || input === undefined || input === false || input === true) return out;\n"
    "  if (Array.isArray(input)) {\n"
    "    for (let i = 0; i < input.length; i++) flatten(input[i], out);\n"
    "    return out;\n"
    "  }\n"
    "  if (typeof input === 'object' && input.type !== undefined) { out.push(input); return out; }\n"
    "  out.push(mkText(input));\n"
    "  return out;\n"
    "}\n"
    "\n"
    "export function h(type, props, children) {\n"
    "  props = props || {};\n"
    "  const key = props.key;\n"
    "  if (isFn(type)) {\n"
    "    let p = props;\n"
    "    if (children !== undefined && children !== null) {\n"
    "      p = Object.assign({}, props);\n"
    "      p.children = frag(children);\n"
    "    }\n"
    "    return { type: type, props: p, key: key, kids: [],\n"
    "             dom: null, hole: null, child: null, inst: null };\n"
    "  }\n"
    "  return { type: type, props: props, key: key, kids: flatten(children, []),\n"
    "           dom: null, hole: null, child: null, inst: null };\n"
    "}\n"
    "\n"
    "export function frag(children) { return h(FRAG, null, children); }\n"
    "export function txt(v) { return mkText(v === null || v === undefined ? '' : v); }\n"
    "\n"
    "/* `for` lists: stamp the key so reconciliation can match across renders. */\n"
    "export function keyed(key, node) {\n"
    "  const vn = asVnode(node);\n"
    "  vn.key = key;\n"
    "  return vn;\n"
    "}\n"
    "\n"
    "function asVnode(v) {\n"
    "  if (v === null || v === undefined || v === false || v === true) return frag(null);\n"
    "  if (Array.isArray(v)) return frag(v);\n"
    "  if (typeof v === 'object' && v.type !== undefined) return v;\n"
    "  return mkText(v);\n"
    "}\n"
    "\n"
    "/* Derived DOM range — never cached, see file header. */\n"
    "function domsOf(vn, out) {\n"
    "  out = out || [];\n"
    "  if (!vn) return out;\n"
    "  if (vn.dom) { out.push(vn.dom); return out; }\n"
    "  if (vn.child) return domsOf(vn.child, out);\n"
    "  if (vn.kids && vn.kids.length) {\n"
    "    for (let i = 0; i < vn.kids.length; i++) domsOf(vn.kids[i], out);\n"
    "    if (out.length) return out;\n"
    "  }\n"
    "  if (vn.hole) out.push(vn.hole);\n"
    "  return out;\n"
    "}\n"
    "function firstDom(vn) { const d = domsOf(vn); return d.length ? d[0] : null; }\n"
    "function lastDom(vn) { const d = domsOf(vn); return d.length ? d[d.length - 1] : null; }\n"
    "function parentDomOf(vn) { const d = firstDom(vn); return d ? d.parentNode : null; }\n"
    "\n"
    "/* ── props / attributes ─────────────────────────────── */\n"
    "\n"
    "const BOOL_ATTR = {\n"
    "  disabled: 'disabled', required: 'required', readonly: 'readonly',\n"
    "  readOnly: 'readonly', selected: 'selected', multiple: 'multiple',\n"
    "  autofocus: 'autofocus', hidden: 'hidden', open: 'open', novalidate: 'novalidate'\n"
    "};\n"
    "\n"
    "function applyStyle(el, oldV, newV) {\n"
    "  if (typeof newV === 'string') { el.style.cssText = newV; return; }\n"
    "  if (typeof oldV === 'object' && oldV) {\n"
    "    for (const k in oldV) if (!newV || !(k in newV)) el.style.removeProperty(k);\n"
    "  }\n"
    "  if (typeof newV === 'object' && newV) {\n"
    "    for (const k in newV) {\n"
    "      const v = newV[k];\n"
    "      if (v === null || v === undefined || v === false) el.style.removeProperty(k);\n"
    "      else el.style.setProperty(k, String(v));\n"
    "    }\n"
    "  } else if (newV === null || newV === undefined) {\n"
    "    el.removeAttribute('style');\n"
    "  }\n"
    "}\n"
    "\n"
    "function setProp(el, k, nv, ov) {\n"
    "  if (k === 'ref') {\n"
    "    if (isFn(nv)) nv(el);\n"
    "    else if (nv && typeof nv === 'object') nv.current = el;\n"
    "    return;\n"
    "  }\n"
    "  if (k.length > 2 && k[0] === 'o' && k[1] === 'n' && (isFn(nv) || isFn(ov))) {\n"
    "    const type = k.slice(2).toLowerCase();\n"
    "    if (!el._cordEv) el._cordEv = {};\n"
    "    const prev = el._cordEv[type];\n"
    "    if (prev) el.removeEventListener(type, prev);\n"
    "    if (isFn(nv)) { el._cordEv[type] = nv; el.addEventListener(type, nv); }\n"
    "    else delete el._cordEv[type];\n"
    "    return;\n"
    "  }\n"
    "  if (k === 'class' || k === 'className') {\n"
    "    const s = nv === null || nv === undefined || nv === false ? '' : String(nv);\n"
    "    if (s) el.setAttribute('class', s); else el.removeAttribute('class');\n"
    "    return;\n"
    "  }\n"
    "  if (k === 'style') { applyStyle(el, ov, nv); return; }\n"
    "  if (k === 'value' && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA' || el.tagName === 'SELECT')) {\n"
    "    const s = nv === null || nv === undefined ? '' : String(nv);\n"
    "    /* Never yank the caret out from under the user mid-typing. */\n"
    "    if (el.value !== s && document.activeElement !== el) el.value = s;\n"
    "    return;\n"
    "  }\n"
    "  if (k === 'checked') { el.checked = !!nv && nv !== 'false'; return; }\n"
    "  const battr = BOOL_ATTR[k];\n"
    "  if (battr) {\n"
    "    if (!!nv && nv !== 'false') el.setAttribute(battr, '');\n"
    "    else el.removeAttribute(battr);\n"
    "    return;\n"
    "  }\n"
    "  if (nv === null || nv === undefined || nv === false) { el.removeAttribute(k); return; }\n"
    "  if (nv === true) { el.setAttribute(k, ''); return; }\n"
    "  el.setAttribute(k, String(nv));\n"
    "}\n"
    "\n"
    "function removeProp(el, k, ov) {\n"
    "  if (k.length > 2 && k[0] === 'o' && k[1] === 'n' && isFn(ov)) {\n"
    "    const type = k.slice(2).toLowerCase();\n"
    "    el.removeEventListener(type, ov);\n"
    "    if (el._cordEv) delete el._cordEv[type];\n"
    "    return;\n"
    "  }\n"
    "  if (k === 'style') { el.removeAttribute('style'); return; }\n"
    "  if (k === 'checked') { el.checked = false; return; }\n"
    "  const battr = BOOL_ATTR[k];\n"
    "  el.removeAttribute(battr ? battr : (k === 'className' ? 'class' : k));\n"
    "}\n"
    "\n"
    "function applyProps(el, oldP, newP) {\n"
    "  oldP = oldP || {};\n"
    "  newP = newP || {};\n"
    "  for (const k in oldP) {\n"
    "    if (k === 'key' || k === 'children') continue;\n"
    "    if (!(k in newP)) removeProp(el, k, oldP[k]);\n"
    "  }\n"
    "  for (const k in newP) {\n"
    "    if (k === 'key' || k === 'children') continue;\n"
    "    const nv = newP[k];\n"
    "    const ov = oldP[k];\n"
    "    if (nv === ov && k !== 'value' && k !== 'checked' && k !== 'style') continue;\n"
    "    setProp(el, k, nv, ov);\n"
    "  }\n"
    "}\n"
    "\n"
    "/* ── component instances ────────────────────────────── */\n"
    "\n"
    "let currentInst = null;\n"
    "let mountDepth = 0;\n"
    "const effectQueue = [];\n"
    "const dirtySet = new Set();\n"
    "const boundaryStack = [];\n"
    "let scheduled = false;\n"
    "\n"
    "function Instance(fn, name, depth) {\n"
    "  this.fn = fn;\n"
    "  this.name = name || 'Component';\n"
    "  this.depth = depth;\n"
    "  this.store = new Map();\n"
    "  this.effects = new Map();\n"
    "  this.pending = [];\n"
    "  this.fxIdx = 0;\n"
    "  this.refIdx = 0;\n"
    "  this.resIdx = 0;\n"
    "  this.disposed = false;\n"
    "  this.vnode = null;\n"
    "  this.api = makeApi(this);\n"
    "}\n"
    "\n"
    "function invalidate(inst) {\n"
    "  if (inst.disposed) return;\n"
    "  dirtySet.add(inst);\n"
    "  if (scheduled) return;\n"
    "  scheduled = true;\n"
    "  Promise.resolve().then(flush);\n"
    "}\n"
    "\n"
    "function makeApi(inst) {\n"
    "  return {\n"
    "    /* Keyed by declaration name: order-independent and stable across\n"
    "       re-renders, unlike positional hooks. */\n"
    "    state(name, init) {\n"
    "      const s = inst.store;\n"
    "      const key = 's:' + name;\n"
    "      if (!s.has(key)) s.set(key, isFn(init) ? init() : init);\n"
    "      return [s.get(key), function (v) {\n"
    "        const cur = s.get(key);\n"
    "        const next = isFn(v) ? v(cur) : v;\n"
    "        if (next === cur) return;\n"
    "        s.set(key, next);\n"
    "        invalidate(inst);\n"
    "      }];\n"
    "    },\n"
    "    effect(fn, deps) {\n"
    "      inst.pending.push({ key: 'e:' + (inst.fxIdx++), fn: fn,\n"
    "                          deps: deps === undefined ? null : deps });\n"
    "    },\n"
    "    ref(init) {\n"
    "      const key = 'r:' + (inst.refIdx++);\n"
    "      if (!inst.store.has(key))\n"
    "        inst.store.set(key, { current: init === undefined ? null : init });\n"
    "      return inst.store.get(key);\n"
    "    },\n"
    "    /* `fetch x = url` — kicks off once, re-renders when it settles. */\n"
    "    resource(name, url, init) {\n"
    "      const key = 'f:' + name;\n"
    "      let slot = inst.store.get(key);\n"
    "      if (!slot || slot.url !== url) {\n"
    "        slot = { url: url, data: init === undefined ? null : init,\n"
    "                 loading: true, error: null };\n"
    "        inst.store.set(key, slot);\n"
    "        fetch(url)\n"
    "          .then((r) => {\n"
    "            if (!r.ok) throw new Error('HTTP ' + r.status + ' ' + url);\n"
    "            const ct = r.headers.get('content-type') || '';\n"
    "            return ct.indexOf('json') >= 0 ? r.json() : r.text();\n"
    "          })\n"
    "          .then((d) => { slot.data = d; slot.loading = false; invalidate(inst); })\n"
    "          .catch((e) => { slot.error = e; slot.loading = false; invalidate(inst); });\n"
    "      }\n"
    "      return slot;\n"
    "    },\n"
    "    params() { return routeState.params; },\n"
    "    query() { return new URLSearchParams(location.search); },\n"
    "    navigate: navigate,\n"
    "    path() { return routeState.path; }\n"
    "  };\n"
    "}\n"
    "\n"
    "function runEffects(inst) {\n"
    "  const pending = inst.pending;\n"
    "  inst.pending = [];\n"
    "  for (let i = 0; i < pending.length; i++) {\n"
    "    const p = pending[i];\n"
    "    const prev = inst.effects.get(p.key);\n"
    "    let changed = true;\n"
    "    if (prev && p.deps && prev.deps && prev.deps.length === p.deps.length) {\n"
    "      changed = false;\n"
    "      for (let j = 0; j < p.deps.length; j++)\n"
    "        if (prev.deps[j] !== p.deps[j]) { changed = true; break; }\n"
    "    }\n"
    "    if (!changed) continue;\n"
    "    if (prev && isFn(prev.cleanup)) { try { prev.cleanup(); } catch (e) { logErr(e); } }\n"
    "    let cleanup = null;\n"
    "    try { cleanup = p.fn(); } catch (e) { logErr(e); }\n"
    "    inst.effects.set(p.key, { deps: p.deps, cleanup: isFn(cleanup) ? cleanup : null });\n"
    "  }\n"
    "}\n"
    "\n"
    "function drainEffects() {\n"
    "  while (effectQueue.length) {\n"
    "    const inst = effectQueue.shift();\n"
    "    if (!inst.disposed) runEffects(inst);\n"
    "  }\n"
    "}\n"
    "\n"
    "function disposeInst(inst) {\n"
    "  inst.disposed = true;\n"
    "  inst.effects.forEach(function (e) {\n"
    "    if (isFn(e.cleanup)) { try { e.cleanup(); } catch (err) { logErr(err); } }\n"
    "  });\n"
    "  inst.effects.clear();\n"
    "  dirtySet.delete(inst);\n"
    "}\n"
    "\n"
    "function renderInst(vn, inst) {\n"
    "  const prev = currentInst;\n"
    "  currentInst = inst;\n"
    "  inst.fxIdx = 0;\n"
    "  inst.refIdx = 0;\n"
    "  inst.resIdx = 0;\n"
    "  let out;\n"
    "  try {\n"
    "    out = inst.fn(vn.props || {}, inst.api);\n"
    "  } catch (e) {\n"
    "    if (boundaryStack.length) {\n"
    "      const b = boundaryStack[boundaryStack.length - 1];\n"
    "      try { b.store.set('eb', { err: e }); invalidate(b); } catch (x) {}\n"
    "      const fb = (b.vnode && b.vnode.props && b.vnode.props.fallback)\n"
    "        ? b.vnode.props.fallback\n"
    "        : h('div', { class: 'cord-runtime-error' },\n"
    "            String(e && e.message ? e.message : e));\n"
    "      out = fb;\n"
    "    } else {\n"
    "      logErr(e);\n"
    "      showErrorOverlay({\n"
    "        title: 'Runtime Error',\n"
    "        diagnostics: [{\n"
    "          level: 'error', file: inst.name || '<runtime>', line: 0, col: 0,\n"
    "          message: e && e.message ? e.message : String(e)\n"
    "        }],\n"
    "        detail: e && e.stack ? e.stack : String(e)\n"
    "      });\n"
    "      out = h('div', { class: 'cord-runtime-error' },\n"
    "              inst.name + ': ' + (e && e.message ? e.message : String(e)));\n"
    "    }\n"
    "  }\n"
    "  currentInst = prev;\n"
    "  effectQueue.push(inst);\n"
    "  return asVnode(out);\n"
    "}\n"
    "\n"
    "/* ── mount / patch / remove ─────────────────────────── */\n"
    "\n"
    "function compatible(a, b) {\n"
    "  if (!a || !b) return false;\n"
    "  return a.type === b.type && a.key === b.key;\n"
    "}\n"
    "\n"
    "function mountVnode(vn, parent, before) {\n"
    "  if (vn.type === '#text') {\n"
    "    vn.dom = document.createTextNode(vn.text);\n"
    "    parent.insertBefore(vn.dom, before || null);\n"
    "    return;\n"
    "  }\n"
    "  if (vn.type === FRAG) {\n"
    "    for (let i = 0; i < vn.kids.length; i++) mountVnode(vn.kids[i], parent, before);\n"
    "    if (!vn.kids.length) {\n"
    "      vn.hole = document.createComment('cord');\n"
    "      parent.insertBefore(vn.hole, before || null);\n"
    "    }\n"
    "    return;\n"
    "  }\n"
    "  if (isFn(vn.type)) {\n"
    "    const inst = new Instance(vn.type, vn.type.cordName || vn.type.name, mountDepth++);\n"
    "    inst.vnode = vn;\n"
    "    vn.inst = inst;\n"
    "    const isEB = inst.name === 'ErrorBoundary';\n"
    "    if (isEB) boundaryStack.push(inst);\n"
    "    try {\n"
    "      vn.child = renderInst(vn, inst);\n"
    "      mountVnode(vn.child, parent, before);\n"
    "    } finally {\n"
    "      if (isEB) boundaryStack.pop();\n"
    "    }\n"
    "    mountDepth--;\n"
    "    return;\n"
    "  }\n"
    "  const el = document.createElement(vn.type);\n"
    "  vn.dom = el;\n"
    "  applyProps(el, null, vn.props);\n"
    "  for (let i = 0; i < vn.kids.length; i++) mountVnode(vn.kids[i], el, null);\n"
    "  parent.insertBefore(el, before || null);\n"
    "}\n"
    "\n"
    "function unmountVnode(vn) {\n"
    "  if (!vn) return;\n"
    "  if (vn.inst) { disposeInst(vn.inst); unmountVnode(vn.child); return; }\n"
    "  if (vn.kids) for (let i = 0; i < vn.kids.length; i++) unmountVnode(vn.kids[i]);\n"
    "}\n"
    "\n"
    "function removeVnode(vn) {\n"
    "  const doms = domsOf(vn);\n"
    "  unmountVnode(vn);\n"
    "  for (let i = 0; i < doms.length; i++) {\n"
    "    const d = doms[i];\n"
    "    if (d.parentNode) d.parentNode.removeChild(d);\n"
    "  }\n"
    "}\n"
    "\n"
    "/* Move an already-mounted range so it sits right after `after`. */\n"
    "function placeAfter(parent, doms, after) {\n"
    "  let ref = after ? after.nextSibling : parent.firstChild;\n"
    "  for (let i = 0; i < doms.length; i++) {\n"
    "    const d = doms[i];\n"
    "    if (d === ref) ref = d.nextSibling;\n"
    "    else parent.insertBefore(d, ref);\n"
    "  }\n"
    "}\n"
    "\n"
    "function patchVnode(oldV, newV, parent) {\n"
    "  if (newV.type === '#text') {\n"
    "    newV.dom = oldV.dom;\n"
    "    if (newV.dom.nodeValue !== newV.text) newV.dom.nodeValue = newV.text;\n"
    "    return;\n"
    "  }\n"
    "  if (newV.type === FRAG) {\n"
    "    newV.hole = oldV.hole;\n"
    "    const anchor = firstDom(oldV);\n"
    "    const prev = anchor ? anchor.previousSibling : null;\n"
    "    patchKids(parent, oldV.kids, newV.kids, prev);\n"
    "    if (!newV.kids.length) {\n"
    "      if (!newV.hole) {\n"
    "        newV.hole = document.createComment('cord');\n"
    "        parent.insertBefore(newV.hole, prev ? prev.nextSibling : parent.firstChild);\n"
    "      }\n"
    "    } else if (newV.hole) {\n"
    "      if (newV.hole.parentNode) newV.hole.parentNode.removeChild(newV.hole);\n"
    "      newV.hole = null;\n"
    "    }\n"
    "    return;\n"
    "  }\n"
    "  if (isFn(newV.type)) {\n"
    "    const inst = oldV.inst;\n"
    "    newV.inst = inst;\n"
    "    inst.vnode = newV;\n"
    "    const oldChild = oldV.child;\n"
    "    const out = renderInst(newV, inst);\n"
    "    newV.child = out;\n"
    "    if (compatible(oldChild, out)) {\n"
    "      patchVnode(oldChild, out, parent);\n"
    "    } else {\n"
    "      mountVnode(out, parent, firstDom(oldChild));\n"
    "      removeVnode(oldChild);\n"
    "    }\n"
    "    return;\n"
    "  }\n"
    "  const el = oldV.dom;\n"
    "  newV.dom = el;\n"
    "  applyProps(el, oldV.props, newV.props);\n"
    "  patchKids(el, oldV.kids, newV.kids, null);\n"
    "}\n"
    "\n"
    "/*\n"
    " * Keyed child reconciliation. `prevDom` is the node immediately before this\n"
    " * range (null = range starts at parent.firstChild), which is what lets a\n"
    " * fragment patch its own slice of a shared parent.\n"
    " */\n"
    "function patchKids(parent, oldKids, newKids, prevDom) {\n"
    "  oldKids = oldKids || [];\n"
    "  newKids = newKids || [];\n"
    "  const used = new Array(oldKids.length).fill(false);\n"
    "  const byKey = new Map();\n"
    "  for (let i = 0; i < oldKids.length; i++) {\n"
    "    const k = oldKids[i].key;\n"
    "    if (k !== undefined && !byKey.has(k)) byKey.set(k, i);\n"
    "  }\n"
    "  let cursor = prevDom;\n"
    "  for (let i = 0; i < newKids.length; i++) {\n"
    "    const nv = newKids[i];\n"
    "    let j = -1;\n"
    "    if (nv.key !== undefined) {\n"
    "      const cand = byKey.get(nv.key);\n"
    "      if (cand !== undefined && !used[cand] && oldKids[cand].type === nv.type) j = cand;\n"
    "    } else if (i < oldKids.length && !used[i] && oldKids[i].key === undefined &&\n"
    "               oldKids[i].type === nv.type) {\n"
    "      j = i;\n"
    "    } else {\n"
    "      for (let s = 0; s < oldKids.length; s++) {\n"
    "        if (!used[s] && oldKids[s].key === undefined && oldKids[s].type === nv.type) { j = s; break; }\n"
    "      }\n"
    "    }\n"
    "    if (j >= 0) {\n"
    "      used[j] = true;\n"
    "      patchVnode(oldKids[j], nv, parent);\n"
    "      placeAfter(parent, domsOf(nv), cursor);\n"
    "    } else {\n"
    "      mountVnode(nv, parent, cursor ? cursor.nextSibling : parent.firstChild);\n"
    "    }\n"
    "    const last = lastDom(nv);\n"
    "    if (last) cursor = last;\n"
    "  }\n"
    "  for (let j = 0; j < oldKids.length; j++) if (!used[j]) removeVnode(oldKids[j]);\n"
    "}\n"
    "\n"
    "function flush() {\n"
    "  scheduled = false;\n"
    "  const list = Array.from(dirtySet);\n"
    "  dirtySet.clear();\n"
    "  /* Parents first: a parent re-render may unmount a queued child. */\n"
    "  list.sort((a, b) => a.depth - b.depth);\n"
    "  for (let i = 0; i < list.length; i++) {\n"
    "    const inst = list[i];\n"
    "    if (inst.disposed) continue;\n"
    "    const vn = inst.vnode;\n"
    "    const oldChild = vn.child;\n"
    "    const parent = parentDomOf(oldChild);\n"
    "    if (!parent) continue;\n"
    "    const out = renderInst(vn, inst);\n"
    "    vn.child = out;\n"
    "    if (compatible(oldChild, out)) {\n"
    "      patchVnode(oldChild, out, parent);\n"
    "    } else {\n"
    "      mountVnode(out, parent, firstDom(oldChild));\n"
    "      removeVnode(oldChild);\n"
    "    }\n"
    "  }\n"
    "  drainEffects();\n"
    "  updateActiveLinks();\n"
    "}\n"
    "\n"
    "/* ── router ─────────────────────────────────────────── */\n"
    "\n"
    "const routeState = { params: {}, path: '/' };\n"
    "const navSubs = new Set();\n"
    "\n"
    "function currentPath() { return location.pathname || '/'; }\n"
    "function emitNav() { navSubs.forEach((f) => f()); }\n"
    "\n"
    "export function navigate(to, opts) {\n"
    "  if (typeof to !== 'string' || !to) return;\n"
    "  const replace = !!(opts && opts.replace);\n"
    "  if (to === currentPath() + location.search) return;\n"
    "  history[replace ? 'replaceState' : 'pushState']({}, '', to);\n"
    "  emitNav();\n"
    "}\n"
    "\n"
    "function segs(p) {\n"
    "  return String(p === undefined || p === null ? '/' : p).split('/').filter(Boolean);\n"
    "}\n"
    "\n"
    "function matchPath(pattern, path) {\n"
    "  if (pattern === '*') return { params: {} };\n"
    "  const ps = segs(pattern);\n"
    "  const xs = segs(path);\n"
    "  const params = {};\n"
    "  for (let i = 0; i < ps.length; i++) {\n"
    "    const seg = ps[i];\n"
    "    if (seg === '*') return { params: params };\n"
    "    if (i >= xs.length) return null;\n"
    "    if (seg[0] === ':') {\n"
    "      params[seg.slice(1)] = decodeURIComponent(xs[i]);\n"
    "      continue;\n"
    "    }\n"
    "    if (seg !== xs[i]) return null;\n"
    "  }\n"
    "  if (xs.length !== ps.length) return null;\n"
    "  return { params: params };\n"
    "}\n"
    "\n"
    "function resolveRoute(routes, path) {\n"
    "  for (let i = 0; i < routes.length; i++) {\n"
    "    const m = matchPath(routes[i].path, path);\n"
    "    if (m) return { route: routes[i], params: m.params };\n"
    "  }\n"
    "  return null;\n"
    "}\n"
    "\n"
    "/* Delegated so a plain <a href=/x> works — no <Link> component needed. */\n"
    "function onDocClick(ev) {\n"
    "  if (ev.defaultPrevented || ev.button !== 0 || ev.metaKey || ev.ctrlKey ||\n"
    "      ev.shiftKey || ev.altKey) return;\n"
    "  let el = ev.target;\n"
    "  while (el && el.nodeType === 1 && el.tagName !== 'A') el = el.parentNode;\n"
    "  if (!el || el.tagName !== 'A') return;\n"
    "  if (el.hasAttribute('target') || el.hasAttribute('download')) return;\n"
    "  const href = el.getAttribute('href');\n"
    "  if (!href || href[0] !== '/') return;\n"
    "  ev.preventDefault();\n"
    "  navigate(href);\n"
    "}\n"
    "\n"
    "/* Mirrors react-router NavLink: chrome CSS keys off [aria-current=page]. */\n"
    "function updateActiveLinks() {\n"
    "  const path = currentPath();\n"
    "  const links = document.querySelectorAll('a[href]');\n"
    "  for (let i = 0; i < links.length; i++) {\n"
    "    const a = links[i];\n"
    "    const href = a.getAttribute('href');\n"
    "    if (!href || href[0] !== '/') continue;\n"
    "    const active = href === '/' ? path === '/'\n"
    "                                : (path === href || path.indexOf(href + '/') === 0);\n"
    "    if (active) a.setAttribute('aria-current', 'page');\n"
    "    else if (a.getAttribute('aria-current') === 'page') a.removeAttribute('aria-current');\n"
    "  }\n"
    "}\n"
    "\n"
    "function RouterRoot(props, $) {\n"
    "  const app = props.app;\n"
    "  const st = $.state('nav', 0);\n"
    "  const bump = st[1];\n"
    "  $.effect(function () {\n"
    "    const fn = function () { bump((n) => n + 1); };\n"
    "    navSubs.add(fn);\n"
    "    window.addEventListener('popstate', fn);\n"
    "    return function () { navSubs.delete(fn); window.removeEventListener('popstate', fn); };\n"
    "  }, []);\n"
    "  const path = currentPath();\n"
    "  routeState.path = path;\n"
    "  const m = resolveRoute(app.routes || [], path);\n"
    "  if (!m) {\n"
    "    routeState.params = {};\n"
    "    return h('div', { class: 'cord-notfound' }, [\n"
    "      h('h1', {}, '404'),\n"
    "      h('p', {}, 'Sin ruta para ' + path),\n"
    "      h('a', { href: '/' }, 'Inicio')\n"
    "    ]);\n"
    "  }\n"
    "  routeState.params = m.params;\n"
    "  const page = h(m.route.component, { params: m.params });\n"
    "  return m.route.layout ? h(m.route.layout, {}, page) : page;\n"
    "}\n"
    "RouterRoot.cordName = 'RouterRoot';\n"
    "\n"
    "/* ── errors ─────────────────────────────────────────── */\n"
    "\n"
    "function logErr(e) { if (window.console) console.error('[cordlang]', e); }\n"
    "\n"
    "export function clearErrorOverlay() {\n"
    "  const box = document.getElementById('cord-overlay');\n"
    "  if (box) box.remove();\n"
    "  if (window.__cordOverlayEsc) {\n"
    "    document.removeEventListener('keydown', window.__cordOverlayEsc);\n"
    "    window.__cordOverlayEsc = null;\n"
    "  }\n"
    "}\n"
    "\n"
    "function el(tag, cls, text) {\n"
    "  const n = document.createElement(tag);\n"
    "  if (cls) n.className = cls;\n"
    "  if (text !== undefined && text !== null) n.textContent = String(text);\n"
    "  return n;\n"
    "}\n"
    "\n"
    "export function showErrorOverlay(payload) {\n"
    "  if (!document.getElementById('cord-overlay-css')) {\n"
    "    const s = document.createElement('style');\n"
    "    s.id = 'cord-overlay-css';\n"
    "    s.textContent = '#cord-overlay{position:fixed;inset:0;z-index:99999;display:flex;"
    "align-items:center;justify-content:center;padding:1.25rem;background:rgba(15,12,10,.55);"
    "backdrop-filter:blur(2px);-webkit-backdrop-filter:blur(2px);color:#fecaca;"
    "font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:0.85rem;overflow:auto}"
    "#cord-overlay .cord-overlay-panel{width:min(36rem,100%);max-height:min(80vh,40rem);"
    "overflow:auto;padding:1.1rem 1.2rem 1.25rem;border-radius:12px;"
    "border:1px solid rgba(248,113,113,.35);border-left:4px solid #f87171;background:#1c1917;"
    "box-shadow:0 24px 60px rgba(0,0,0,.45)}"
    "#cord-overlay .cord-overlay-head{display:flex;align-items:center;"
    "justify-content:space-between;gap:1rem;margin-bottom:0.85rem}"
    "#cord-overlay .cord-overlay-title{color:#fecaca;font-size:1.05rem;font-weight:700}"
    "#cord-overlay .cord-overlay-diag{margin:0.65rem 0}"
    "#cord-overlay .cord-overlay-loc{color:#fdba74;margin-bottom:0.25rem}"
    "#cord-overlay .cord-overlay-msg{color:#fee2e2;white-space:pre-wrap}"
    "#cord-overlay .cord-overlay-code,#cord-overlay .cord-overlay-hint{"
    "color:#a8a29e;margin-top:0.35rem;font-size:0.8rem}"
    "#cord-overlay .cord-overlay-hint{color:#86efac}"
    "#cord-overlay .cord-overlay-frame,#cord-overlay .cord-overlay-detail{"
    "margin:0.85rem 0 0;padding:0.65rem 0.75rem;background:#0c0a09;border-radius:8px;"
    "overflow:auto;white-space:pre-wrap;color:#e7e5e4}"
    "#cord-overlay .cord-overlay-line{display:flex;gap:0.85rem;padding:0.1rem 0.25rem}"
    "#cord-overlay .cord-overlay-line-err{background:rgba(248,113,113,.18);color:#fecaca}"
    "#cord-overlay .cord-overlay-ln{width:2.5rem;text-align:right;color:#78716c;"
    "user-select:none;flex-shrink:0}"
    "#cord-overlay .cord-overlay-src{white-space:pre}"
    "#cord-overlay .cord-overlay-close{border:none;border-radius:6px;"
    "padding:0.35rem 0.9rem;cursor:pointer;background:#fecaca;color:#1c1917;font-weight:600}';\n"
    "    document.head.appendChild(s);\n"
    "  }\n"
    "  const p = payload || {};\n"
    "  const diags = Array.isArray(p.diagnostics) ? p.diagnostics : [];\n"
    "  const title = p.title || (diags.length ? 'Failed to compile' : 'Error');\n"
    "  let box = document.getElementById('cord-overlay');\n"
    "  if (!box) {\n"
    "    box = document.createElement('div');\n"
    "    box.id = 'cord-overlay';\n"
    "    document.body.appendChild(box);\n"
    "  }\n"
    "  box.textContent = '';\n"
    "  box.className = 'cord-overlay-open';\n"
    "  box.setAttribute('role', 'dialog');\n"
    "  box.setAttribute('aria-modal', 'true');\n"
    "  box.setAttribute('aria-label', title);\n"
    "  box.onclick = function (ev) {\n"
    "    if (ev.target === box) clearErrorOverlay();\n"
    "  };\n"
    "  const panel = el('div', 'cord-overlay-panel');\n"
    "  panel.onclick = function (ev) { ev.stopPropagation(); };\n"
    "  const head = el('div', 'cord-overlay-head');\n"
    "  head.appendChild(el('strong', 'cord-overlay-title', title));\n"
    "  const close = el('button', 'cord-overlay-close', 'Close');\n"
    "  close.type = 'button';\n"
    "  close.onclick = function () { clearErrorOverlay(); };\n"
    "  head.appendChild(close);\n"
    "  panel.appendChild(head);\n"
    "  for (let i = 0; i < diags.length; i++) {\n"
    "    const d = diags[i] || {};\n"
    "    const block = el('div', 'cord-overlay-diag');\n"
    "    const loc = (d.file || '<unknown>') +\n"
    "      (d.line > 0 ? (':' + d.line + (d.col > 0 ? (':' + d.col) : '')) : '');\n"
    "    block.appendChild(el('div', 'cord-overlay-loc', loc));\n"
    "    block.appendChild(el('div', 'cord-overlay-msg', d.message || ''));\n"
    "    if (d.code) block.appendChild(el('div', 'cord-overlay-code', 'code: ' + d.code));\n"
    "    if (d.hint) block.appendChild(el('div', 'cord-overlay-hint', 'hint: ' + d.hint));\n"
    "    panel.appendChild(block);\n"
    "  }\n"
    "  const frame = p.frame;\n"
    "  if (frame && Array.isArray(frame.lines) && frame.lines.length) {\n"
    "    const pre = el('pre', 'cord-overlay-frame');\n"
    "    const start = frame.startLine > 0 ? frame.startLine : 1;\n"
    "    const errLine = (diags[0] && diags[0].line > 0) ? diags[0].line : 0;\n"
    "    for (let i = 0; i < frame.lines.length; i++) {\n"
    "      const ln = start + i;\n"
    "      const row = el('div', ln === errLine ? 'cord-overlay-line cord-overlay-line-err' : 'cord-overlay-line');\n"
    "      row.appendChild(el('span', 'cord-overlay-ln', String(ln)));\n"
    "      row.appendChild(el('span', 'cord-overlay-src', frame.lines[i]));\n"
    "      pre.appendChild(row);\n"
    "    }\n"
    "    panel.appendChild(pre);\n"
    "  }\n"
    "  if (p.detail) panel.appendChild(el('pre', 'cord-overlay-detail', p.detail));\n"
    "  if (!diags.length && !p.detail) panel.appendChild(el('pre', 'cord-overlay-detail', String(p.message || '')));\n"
    "  box.appendChild(panel);\n"
    "  if (!window.__cordOverlayEsc) {\n"
    "    window.__cordOverlayEsc = function (ev) {\n"
    "      if (ev && ev.key === 'Escape') clearErrorOverlay();\n"
    "    };\n"
    "    document.addEventListener('keydown', window.__cordOverlayEsc);\n"
    "  }\n"
    "}\n"
    "\n"
    "export function showOverlay(title, detail) {\n"
    "  showErrorOverlay({ title: title || 'Error', detail: detail, diagnostics: [] });\n"
    "}\n"
    "\n"
    "export function moduleError(file, message) {\n"
    "  showErrorOverlay({\n"
    "    title: 'Failed to compile',\n"
    "    diagnostics: [{ level: 'error', file: file || '<unknown>', line: 0, col: 0,\n"
    "                    message: message || 'error' }]\n"
    "  });\n"
    "  throw new Error((file || 'cordlang') + ': ' + (message || 'error'));\n"
    "}\n"
    "\n"
    "export function showCompileError(payload) {\n"
    "  showErrorOverlay(payload || {});\n"
    "  const d0 = payload && payload.diagnostics && payload.diagnostics[0];\n"
    "  const msg = d0 && d0.message ? d0.message : 'compile error';\n"
    "  const file = d0 && d0.file ? d0.file : 'cordlang';\n"
    "  throw new Error(file + ': ' + msg);\n"
    "}\n"
    "\n"
    "if (typeof window !== 'undefined' && !window.__cordErrHooks) {\n"
    "  window.__cordErrHooks = 1;\n"
    "  window.addEventListener('error', function (ev) {\n"
    "    if (!ev || ev.defaultPrevented) return;\n"
    "    if (document.getElementById('cord-overlay')) return;\n"
    "    const err = ev.error;\n"
    "    showErrorOverlay({\n"
    "      title: 'Runtime Error',\n"
    "      diagnostics: [{ level: 'error', file: ev.filename || '<runtime>',\n"
    "                      line: ev.lineno || 0, col: ev.colno || 0,\n"
    "                      message: err && err.message ? err.message : (ev.message || 'error') }],\n"
    "      detail: err && err.stack ? err.stack : undefined\n"
    "    });\n"
    "  });\n"
    "  window.addEventListener('unhandledrejection', function (ev) {\n"
    "    if (!ev) return;\n"
    "    if (document.getElementById('cord-overlay')) return;\n"
    "    const r = ev.reason;\n"
    "    showErrorOverlay({\n"
    "      title: 'Unhandled Rejection',\n"
    "      diagnostics: [{ level: 'error', file: '<runtime>', line: 0, col: 0,\n"
    "                      message: r && r.message ? r.message : String(r) }],\n"
    "      detail: r && r.stack ? r.stack : undefined\n"
    "    });\n"
    "  });\n"
    "}\n"
    "\n"
    "/* ── mount ──────────────────────────────────────────── */\n"
    "\n"
    "export function component(name, fn) {\n"
    "  fn.cordName = name;\n"
    "  return fn;\n"
    "}\n"
    "\n"
    "function asComponent(mod) {\n"
    "  if (isFn(mod)) return mod;\n"
    "  if (mod && isFn(mod.default)) return mod.default;\n"
    "  return component('Missing', function () {\n"
    "    return h('div', { class: 'cord-runtime-error' },\n"
    "             'El modulo de entrada no exporta un componente ni rutas.');\n"
    "  });\n"
    "}\n"
    "\n"
    "let booted = false;\n"
    "let mountHost = null;\n"
    "let mountRoot = null;\n"
    "\n"
    "export function remount(mod, el) {\n"
    "  clearErrorOverlay();\n"
    "  const target = el || mountHost || document.getElementById('app') || document.body;\n"
    "  if (mountRoot) {\n"
    "    try { unmountVnode(mountRoot); } catch (e) { logErr(e); }\n"
    "    mountRoot = null;\n"
    "  }\n"
    "  target.textContent = '';\n"
    "  return mount(mod, target);\n"
    "}\n"
    "\n"
    "export function mount(mod, el) {\n"
    "  clearErrorOverlay();\n"
    "  const target = el || document.getElementById('app') || document.body;\n"
    "  mountHost = target;\n"
    "  if (!booted) {\n"
    "    document.addEventListener('click', onDocClick);\n"
    "    booted = true;\n"
    "  }\n"
    "  const app = mod && mod.__cord === 'app' ? mod : null;\n"
    "  const root = app ? h(RouterRoot, { app: app }) : h(asComponent(mod), {});\n"
    "  target.textContent = '';\n"
    "  mountDepth = 0;\n"
    "  mountVnode(root, target, null);\n"
    "  mountRoot = root;\n"
    "  drainEffects();\n"
    "  updateActiveLinks();\n"
    "  return root;\n"
    "}\n"
    "\n"
    "export function ErrorBoundary(props, $) {\n"
    "  const st = $.state('eb', { err: null });\n"
    "  if (st[0].err) {\n"
    "    return props.fallback || h('div', { class: 'cord-runtime-error' },\n"
    "      String(st[0].err && st[0].err.message ? st[0].err.message : st[0].err));\n"
    "  }\n"
    "  return props.children || null;\n"
    "}\n"
    "ErrorBoundary.cordName = 'ErrorBoundary';\n"
    "\n"
    "export function Portal(props, $) {\n"
    "  const kids = props.children;\n"
    "  $.effect(function () {\n"
    "    const host = document.createElement('div');\n"
    "    host.className = 'cord-portal';\n"
    "    const target = (props.target === 'body' || !props.target)\n"
    "      ? document.body\n"
    "      : (document.querySelector(props.target) || document.body);\n"
    "    target.appendChild(host);\n"
    "    const vn = asVnode(kids);\n"
    "    mountVnode(vn, host, null);\n"
    "    drainEffects();\n"
    "    return function () {\n"
    "      try { unmountVnode(vn); } catch (e) {}\n"
    "      host.remove();\n"
    "    };\n"
    "  }, [kids]);\n"
    "  return frag(null);\n"
    "}\n"
    "Portal.cordName = 'Portal';\n"
    "\n"
    "export function Suspense(props) {\n"
    "  if (props.loading) return props.fallback || null;\n"
    "  return props.children || null;\n"
    "}\n"
    "Suspense.cordName = 'Suspense';\n"
    "\n"
    "export default { h: h, frag: frag, txt: txt, mount: mount, remount: remount,\n"
    "                 component: component, navigate: navigate, Frag: FRAG,\n"
    "                 ErrorBoundary: ErrorBoundary, Portal: Portal, Suspense: Suspense };\n";

static char HMR_CLIENT_BUF[4096];

const char *esm_runtime_js(void) { return RUNTIME_JS; }

const char *esm_hmr_client_js(const char *entry_url) {
  const char *entry = entry_url && *entry_url ? entry_url : "/src/app.cord";
  char esc[512];
  size_t j = 0;
  esc[j++] = '\'';
  for (const char *p = entry; *p && j + 3 < sizeof(esc); p++) {
    if (*p == '\'' || *p == '\\') esc[j++] = '\\';
    esc[j++] = *p;
  }
  esc[j++] = '\'';
  esc[j] = '\0';
  snprintf(
      HMR_CLIENT_BUF, sizeof(HMR_CLIENT_BUF),
      "/* Cordlang soft update / reload: SSE from the compiler process. */\n"
      "(function () {\n"
      "  var ENTRY = %s;\n"
      "  var es = null;\n"
      "  var retry = 0;\n"
      "  var updating = 0;\n"
      "  function fullReload() { location.reload(); }\n"
      "  function softUpdate(url) {\n"
      "    if (updating) return;\n"
      "    updating = 1;\n"
      "    var bust = Date.now();\n"
      "    var entryUrl = ENTRY + (ENTRY.indexOf('?') >= 0 ? '&' : '?') + 't=' + bust;\n"
      "    import('/@cord/runtime.js').then(function (rt) {\n"
      "      return import(entryUrl).then(function (mod) {\n"
      "        var el = document.getElementById('app');\n"
      "        if (!el || !rt.remount) { fullReload(); return; }\n"
      "        /* Success: remount replaces UI and clears any error modal. */\n"
      "        rt.remount(mod.default !== undefined ? mod.default : mod, el);\n"
      "      });\n"
      "    }).catch(function () {\n"
      "      /* Compile error: keep last good page; modal already painted. */\n"
      "    }).then(function () { updating = 0; });\n"
      "  }\n"
      "  function connect() {\n"
      "    try { es = new EventSource('/@cord/hmr'); } catch (e) { return; }\n"
      "    es.onopen = function () { retry = 0; };\n"
      "    es.onmessage = function (ev) {\n"
      "      if (!ev || !ev.data) return;\n"
      "      if (ev.data === 'reload') { fullReload(); return; }\n"
      "      if (ev.data.indexOf('update:') === 0) softUpdate(ev.data.slice(7));\n"
      "    };\n"
      "    es.onerror = function () {\n"
      "      if (es) { es.close(); es = null; }\n"
      "      retry = Math.min(retry + 1, 10);\n"
      "      setTimeout(connect, 250 * retry);\n"
      "    };\n"
      "  }\n"
      "  connect();\n"
      "})();\n",
      esc);
  return HMR_CLIENT_BUF;
}

/*
 * Vite-style shell: no inline bootstrap. The entry .cord is loaded as
 *   <script type="module" src="/src/app.cord"></script>
 * and self-mounts into #app (see esm_generate_module). /@cord/client is the
 * reload client (same role as /@vite/client).
 */
char *esm_index_html(const char *lang, const char *title, const char *entry_url,
                     int has_site_css, int has_site_js, int has_favicon,
                     int has_logo_svg, int with_hmr) {
  char lang_e[64];
  char title_e[512];
  html_escape_to(lang_e, sizeof(lang_e), lang && *lang ? lang : "en");
  html_escape_to(title_e, sizeof(title_e),
                 title && *title ? title : "Cordlang App");

  const char *entry = entry_url && *entry_url ? entry_url : "/src/app.cord";

  size_t cap = 4096 + strlen(entry);
  char *out = malloc(cap);
  if (!out) return NULL;

  snprintf(
      out, cap,
      "<!doctype html>\n"
      "<html lang=\"%s\">\n"
      "  <head>\n"
      "    <meta charset=\"UTF-8\" />\n"
      "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
      "%s"
      "%s"
      "    <title>%s</title>\n"
      "    <link rel=\"modulepreload\" href=\"/@cord/runtime.js\" />\n"
      "    <link rel=\"stylesheet\" href=\"/@cord/styles.css\" />\n"
      "%s"
      "%s"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"app\"></div>\n"
      "    <script type=\"module\" src=\"%s\"></script>\n"
      "%s"
      "%s"
      "  </body>\n"
      "</html>\n",
      lang_e,
      has_favicon ? "    <link rel=\"icon\" href=\"/favicon.ico\" sizes=\"any\" />\n"
                  : "",
      has_logo_svg
          ? "    <link rel=\"icon\" href=\"/logo.svg\" type=\"image/svg+xml\" />\n"
          : "",
      title_e,
      has_site_css ? "    <link rel=\"stylesheet\" href=\"/site.css\" />\n" : "",
      has_site_js
          ? "    <script>\n"
            "(function(){try{var t=localStorage.getItem(\"cord-docs-theme\");"
            "if(t!==\"dark\"&&t!==\"light\")"
            "t=window.matchMedia(\"(prefers-color-scheme: dark)\").matches?"
            "\"dark\":\"light\";"
            "document.documentElement.setAttribute(\"data-theme\",t);"
            "document.documentElement.style.colorScheme=t;"
            "}catch(e){}})();\n"
            "    </script>\n"
          : "",
      entry,
      has_site_js ? "    <script type=\"module\" src=\"/site.js\"></script>\n" : "",
      with_hmr
          ? "    <script>\n"
            "    (function(){function load(){import('/@cord/client').catch(function(){})}"
            "if('requestIdleCallback' in window)requestIdleCallback(load,{timeout:2000});"
            "else setTimeout(load,1);})();\n"
            "    </script>\n"
          : "");
  return out;
}

char *esm_error_module(const char *message) {
  DiagList diags;
  diag_list_init(&diags);
  diag_emit(&diags, DIAG_ERROR, "cordlang", 0, 0, "%s",
            message ? message : "error desconocido");
  char *out = esm_error_module_from_diags(&diags, NULL, 0);
  diag_list_free(&diags);
  return out;
}

static void frame_json_append(char **buf, size_t *len, size_t *cap,
                              const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (*len + n + 1 > *cap) {
    size_t ncap = *cap ? *cap * 2 : 512;
    while (ncap < *len + n + 1) ncap *= 2;
    char *nb = realloc(*buf, ncap);
    if (!nb) return;
    *buf = nb;
    *cap = ncap;
  }
  memcpy(*buf + *len, s, n);
  *len += n;
  (*buf)[*len] = '\0';
}

static void frame_json_escape(char **buf, size_t *len, size_t *cap,
                              const char *s) {
  if (!s) return;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    char tmp[8];
    switch (*p) {
      case '"':
        frame_json_append(buf, len, cap, "\\\"");
        break;
      case '\\':
        frame_json_append(buf, len, cap, "\\\\");
        break;
      case '\n':
        frame_json_append(buf, len, cap, "\\n");
        break;
      case '\r':
        frame_json_append(buf, len, cap, "\\r");
        break;
      case '\t':
        frame_json_append(buf, len, cap, "\\t");
        break;
      default:
        if (*p < 0x20) {
          snprintf(tmp, sizeof(tmp), "\\u%04x", *p);
          frame_json_append(buf, len, cap, tmp);
        } else {
          tmp[0] = (char)*p;
          tmp[1] = '\0';
          frame_json_append(buf, len, cap, tmp);
        }
        break;
    }
  }
}

/* Build { "file", "startLine", "lines": [...] } around the first diag with line>0. */
static char *build_frame_json(const DiagList *diags, const char *source,
                              size_t source_len) {
  if (!diags || !diags->len || !source) return NULL;
  const Diagnostic *hit = NULL;
  for (size_t i = 0; i < diags->len; i++) {
    if (diags->items[i].line > 0) {
      hit = &diags->items[i];
      break;
    }
  }
  if (!hit) hit = &diags->items[0];
  if (hit->line <= 0) return NULL;

  int err_line = hit->line;
  int start = err_line - 3;
  if (start < 1) start = 1;
  int end = err_line + 3;

  /* Collect lines 1..end */
  const char *p = source;
  size_t remain = source_len;
  int lineno = 1;
  char *out = NULL;
  size_t len = 0, cap = 0;
  frame_json_append(&out, &len, &cap, "{\"file\":\"");
  frame_json_escape(&out, &len, &cap, hit->file ? hit->file : "<input>");
  {
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "\",\"startLine\":%d,\"lines\":[", start);
    frame_json_append(&out, &len, &cap, hdr);
  }
  int wrote = 0;
  while (remain > 0 && lineno <= end) {
    const char *nl = memchr(p, '\n', remain);
    size_t line_len = nl ? (size_t)(nl - p) : remain;
    if (lineno >= start) {
      if (wrote) frame_json_append(&out, &len, &cap, ",");
      frame_json_append(&out, &len, &cap, "\"");
      /* strip trailing \r */
      size_t use = line_len;
      if (use > 0 && p[use - 1] == '\r') use--;
      char *tmp = malloc(use + 1);
      if (tmp) {
        memcpy(tmp, p, use);
        tmp[use] = '\0';
        frame_json_escape(&out, &len, &cap, tmp);
        free(tmp);
      }
      frame_json_append(&out, &len, &cap, "\"");
      wrote = 1;
    }
    if (!nl) break;
    size_t step = line_len + 1;
    p += step;
    remain -= step;
    lineno++;
  }
  frame_json_append(&out, &len, &cap, "]}");
  return out;
}

char *esm_error_module_from_diags(const DiagList *diags, const char *source,
                                  size_t source_len) {
  char *diag_json = diag_format_json(diags);
  char *frame_json = build_frame_json(diags, source, source_len);

  size_t payload_cap = (diag_json ? strlen(diag_json) : 2) +
                       (frame_json ? strlen(frame_json) : 0) + 128;
  char *payload = malloc(payload_cap);
  if (!payload) {
    free(diag_json);
    free(frame_json);
    return NULL;
  }
  if (frame_json) {
    snprintf(payload, payload_cap,
             "{\"title\":\"Failed to compile\",\"diagnostics\":%s,\"frame\":%s}",
             diag_json ? diag_json : "[]", frame_json);
  } else {
    snprintf(payload, payload_cap,
             "{\"title\":\"Failed to compile\",\"diagnostics\":%s}",
             diag_json ? diag_json : "[]");
  }
  free(diag_json);
  free(frame_json);

  char *esc = js_escape_dq_dup(payload);
  free(payload);
  if (!esc) return NULL;

  size_t need = strlen(esc) + 256;
  char *out = malloc(need);
  if (!out) {
    free(esc);
    return NULL;
  }
  snprintf(out, need,
           "import { showCompileError } from '/@cord/runtime.js';\n"
           "showCompileError(JSON.parse(\"%s\"));\n"
           "export default null;\n",
           esc);
  free(esc);
  return out;
}
