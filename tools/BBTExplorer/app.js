"use strict";

(function () {

const { parseBBT, TagType, TagTypeName } = window.BBT;

const dropZone = document.getElementById("drop-zone");
const placeholder = document.getElementById("placeholder");
const tabsEl = document.getElementById("tabs");
const contentEl = document.getElementById("content");
const statusEl = document.getElementById("status");
const fileInput = document.getElementById("file-input");
const filterInput = document.getElementById("filter");
const expandAllBtn = document.getElementById("expand-all");
const collapseAllBtn = document.getElementById("collapse-all");
const saveBtn = document.getElementById("save");

const docs = [];
let activeDocId = null;
let docIdSeq = 0;

function setStatus(text, kind = "") {
    statusEl.textContent = text;
    statusEl.className = kind;
}

function formatBytes(n) {
    if (n < 1024) return `${n} B`;
    if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KiB`;
    return `${(n / 1024 / 1024).toFixed(2)} MiB`;
}

function escapeHTML(s) {
    return String(s)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#39;");
}

function formatValue(type, value) {
    switch (type) {
        case TagType.T_BOOL:
            return { html: `<span class="value bool">${value ? "true" : "false"}</span>`, text: String(value) };
        case TagType.T_STRING: {
            const text = JSON.stringify(value);
            return { html: `<span class="value string">${escapeHTML(text)}</span>`, text };
        }
        case TagType.T_INT8:
        case TagType.T_INT16:
        case TagType.T_INT32:
        case TagType.T_UINT8:
        case TagType.T_UINT16:
        case TagType.T_UINT32:
            return { html: `<span class="value number">${value}</span>`, text: String(value) };
        case TagType.T_INT64:
        case TagType.T_UINT64:
            return { html: `<span class="value number">${value.toString()}</span>`, text: value.toString() };
        case TagType.T_FLOAT:
        case TagType.T_DOUBLE: {
            const text = Number.isFinite(value) ? value.toString() : String(value);
            return { html: `<span class="value number">${escapeHTML(text)}</span>`, text };
        }
        case TagType.T_VECTOR2_FLOAT:
        case TagType.T_VECTOR2_INT:
        case TagType.T_VECTOR2_DOUBLE: {
            const text = `(${value.x}, ${value.y})`;
            return { html: `<span class="value vec">${escapeHTML(text)}</span>`, text };
        }
        default:
            return { html: "", text: "" };
    }
}

function hexdump(bytes, maxBytes = 1024) {
    const limit = Math.min(bytes.length, maxBytes);
    const lines = [];
    for (let offset = 0; offset < limit; offset += 16) {
        const chunk = bytes.subarray(offset, Math.min(offset + 16, limit));
        const hexParts = [];
        const asciiParts = [];
        for (let i = 0; i < 16; i++) {
            if (i < chunk.length) {
                hexParts.push(chunk[i].toString(16).padStart(2, "0"));
                const b = chunk[i];
                asciiParts.push(b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : ".");
            } else {
                hexParts.push("  ");
                asciiParts.push(" ");
            }
            if (i === 7) hexParts.push("");
        }
        const addr = offset.toString(16).padStart(8, "0");
        lines.push(`${addr}  ${hexParts.join(" ")}  <span class="ascii">${escapeHTML(asciiParts.join(""))}</span>`);
    }
    if (bytes.length > maxBytes) {
        lines.push(`... (${bytes.length - maxBytes} more bytes truncated)`);
    }
    return lines.join("\n");
}

function sortedCompoundEntries(entries) {
    return [...entries].sort((a, b) => a.name.localeCompare(b.name));
}

const EDITABLE_TYPES = new Set([
    TagType.T_BOOL,
    TagType.T_INT8, TagType.T_INT16, TagType.T_INT32, TagType.T_INT64,
    TagType.T_UINT8, TagType.T_UINT16, TagType.T_UINT32, TagType.T_UINT64,
    TagType.T_FLOAT, TagType.T_DOUBLE,
    TagType.T_STRING,
    TagType.T_VECTOR2_FLOAT, TagType.T_VECTOR2_INT, TagType.T_VECTOR2_DOUBLE,
]);

const INT_RANGES = {
    [TagType.T_INT8]:   { min: -0x80n,                 max: 0x7fn },
    [TagType.T_INT16]:  { min: -0x8000n,               max: 0x7fffn },
    [TagType.T_INT32]:  { min: -0x80000000n,           max: 0x7fffffffn },
    [TagType.T_INT64]:  { min: -0x8000000000000000n,   max: 0x7fffffffffffffffn },
    [TagType.T_UINT8]:  { min: 0n,                     max: 0xffn },
    [TagType.T_UINT16]: { min: 0n,                     max: 0xffffn },
    [TagType.T_UINT32]: { min: 0n,                     max: 0xffffffffn },
    [TagType.T_UINT64]: { min: 0n,                     max: 0xffffffffffffffffn },
};

function parseEditedValue(type, text) {
    switch (type) {
        case TagType.T_BOOL: {
            const t = text.trim().toLowerCase();
            if (t === "true" || t === "1")  return true;
            if (t === "false" || t === "0") return false;
            throw new Error("expected true/false");
        }
        case TagType.T_STRING:
            return text;
        case TagType.T_FLOAT:
        case TagType.T_DOUBLE: {
            const n = Number(text.trim());
            if (!Number.isFinite(n)) throw new Error("not a finite number");
            return n;
        }
        case TagType.T_INT8:
        case TagType.T_INT16:
        case TagType.T_INT32:
        case TagType.T_UINT8:
        case TagType.T_UINT16:
        case TagType.T_UINT32: {
            const n = Number(text.trim());
            if (!Number.isInteger(n)) throw new Error("not an integer");
            const r = INT_RANGES[type];
            const bn = BigInt(n);
            if (bn < r.min || bn > r.max) throw new Error(`out of range for ${TagTypeName[type]}`);
            return n;
        }
        case TagType.T_INT64:
        case TagType.T_UINT64: {
            let bn;
            try { bn = BigInt(text.trim()); }
            catch { throw new Error("not an integer"); }
            const r = INT_RANGES[type];
            if (bn < r.min || bn > r.max) throw new Error(`out of range for ${TagTypeName[type]}`);
            return bn;
        }
        case TagType.T_VECTOR2_FLOAT:
        case TagType.T_VECTOR2_DOUBLE: {
            const parts = text.split(",").map(s => s.trim());
            if (parts.length !== 2) throw new Error("expected 'x, y'");
            const x = Number(parts[0]);
            const y = Number(parts[1]);
            if (!Number.isFinite(x) || !Number.isFinite(y)) throw new Error("non-finite component");
            return { x, y };
        }
        case TagType.T_VECTOR2_INT: {
            const parts = text.split(",").map(s => s.trim());
            if (parts.length !== 2) throw new Error("expected 'x, y'");
            const x = Number(parts[0]);
            const y = Number(parts[1]);
            if (!Number.isInteger(x) || !Number.isInteger(y)) throw new Error("components must be integers");
            if (x < -0x80000000 || x > 0x7fffffff || y < -0x80000000 || y > 0x7fffffff) throw new Error("out of int32 range");
            return { x, y };
        }
        default:
            throw new Error("not editable");
    }
}

function initialEditText(type, value) {
    switch (type) {
        case TagType.T_BOOL: return value ? "true" : "false";
        case TagType.T_STRING: return String(value);
        case TagType.T_INT64:
        case TagType.T_UINT64: return value.toString();
        case TagType.T_VECTOR2_FLOAT:
        case TagType.T_VECTOR2_INT:
        case TagType.T_VECTOR2_DOUBLE: return `${value.x}, ${value.y}`;
        default: return String(value);
    }
}

function beginEdit(valInner, holder, onEdit) {
    const originalHTML = valInner.innerHTML;
    const type = holder.type;

    let input;
    if (type === TagType.T_BOOL) {
        input = document.createElement("select");
        for (const opt of ["true", "false"]) {
            const o = document.createElement("option");
            o.value = opt; o.textContent = opt;
            input.appendChild(o);
        }
        input.value = holder.value ? "true" : "false";
    } else {
        input = document.createElement("input");
        input.type = "text";
        input.value = initialEditText(type, holder.value);
        input.spellcheck = false;
        input.autocomplete = "off";
    }
    input.className = "edit-input";

    valInner.textContent = "";
    valInner.appendChild(input);
    input.focus();
    if (input.select) input.select();

    let settled = false;

    const restore = (html) => {
        valInner.innerHTML = html;
    };

    const commit = () => {
        if (settled) return;
        settled = true;
        try {
            const nv = parseEditedValue(type, input.value);
            holder.value = nv;
            if (onEdit) onEdit(nv);
            const formatted = formatValue(type, nv);
            restore(formatted.html);
            setStatus("Edited", "ok");
        } catch (err) {
            settled = false;
            setStatus(`Edit: ${err.message}`, "error");
            input.focus();
            if (input.select) input.select();
        }
    };

    const cancel = () => {
        if (settled) return;
        settled = true;
        restore(originalHTML);
    };

    input.addEventListener("blur", commit);
    input.addEventListener("keydown", (e) => {
        if (e.key === "Enter") {
            e.preventDefault();
            commit();
        } else if (e.key === "Escape") {
            e.preventDefault();
            cancel();
        }
    });
    input.addEventListener("click", (e) => e.stopPropagation());
    input.addEventListener("mousedown", (e) => e.stopPropagation());
}

function renderNode(name, holder, isRoot = false, onEdit = null) {
    const type = holder.type;
    const value = holder.value;
    const typeName = TagTypeName[type] || `T_UNKNOWN(${type})`;
    const node = document.createElement("div");
    node.className = "node" + (isRoot ? " root" : "");
    node.dataset.name = name || "";
    node.dataset.type = typeName;

    const head = document.createElement("div");
    head.className = "node-head";

    let children = null;
    let summary = "";
    let valueHTML = "";

    if (type === TagType.T_COMPOUND) {
        const entries = sortedCompoundEntries(value || []);
        summary = `${entries.length} ${entries.length === 1 ? "entry" : "entries"}`;
        if (entries.length > 0) {
            children = document.createElement("div");
            children.className = "children collapsed";
            for (const entry of entries) {
                children.appendChild(renderNode(entry.name, entry));
            }
        }
    } else if (type === TagType.T_LIST) {
        const list = value || { elementType: TagType.T_END, items: [] };
        const elemTypeName = TagTypeName[list.elementType] || `T_UNKNOWN(${list.elementType})`;
        summary = `${list.items.length} × ${elemTypeName}`;
        if (list.items.length > 0) {
            children = document.createElement("div");
            children.className = "children collapsed";
            list.items.forEach((item, i) => {
                children.appendChild(renderNode(`[${i}]`, item));
            });
        }
    } else if (type === TagType.T_BYTEARRAY) {
        const bytes = value || new Uint8Array();
        summary = formatBytes(bytes.length);
        if (bytes.length > 0) {
            children = document.createElement("div");
            children.className = "children collapsed";
            const dump = document.createElement("div");
            dump.className = "hexdump";
            dump.innerHTML = hexdump(bytes);
            children.appendChild(dump);
        }
    } else {
        const formatted = formatValue(type, value);
        valueHTML = formatted.html;
    }

    const collapsible = children !== null;
    if (collapsible) head.classList.add("collapsible");

    const toggle = document.createElement("span");
    toggle.className = "toggle" + (collapsible ? "" : " leaf");
    toggle.textContent = collapsible ? "▶" : "•";
    head.appendChild(toggle);

    const nameEl = document.createElement("span");
    nameEl.className = "name";
    nameEl.textContent = name || "(unnamed)";
    head.appendChild(nameEl);

    const typeEl = document.createElement("span");
    typeEl.className = `type ${typeName}`;
    typeEl.textContent = typeName.replace(/^T_/, "");
    head.appendChild(typeEl);

    if (summary) {
        const sumEl = document.createElement("span");
        sumEl.className = "summary";
        sumEl.textContent = summary;
        head.appendChild(sumEl);
    }

    if (valueHTML) {
        const valEl = document.createElement("span");
        valEl.innerHTML = `= ${valueHTML}`;
        head.appendChild(valEl);
        if (EDITABLE_TYPES.has(type)) {
            const valInner = valEl.querySelector(".value");
            if (valInner) {
                valInner.classList.add("editable");
                valInner.title = "Click to edit";
                valInner.addEventListener("click", (e) => {
                    e.stopPropagation();
                    beginEdit(valInner, holder, (nv) => {
                        markActiveDocDirty();
                        if (onEdit) onEdit(nv);
                    });
                });
            }
        }
    }

    node.appendChild(head);
    if (children) {
        node.appendChild(children);
        head.addEventListener("click", (e) => {
            e.stopPropagation();
            const collapsed = children.classList.toggle("collapsed");
            toggle.textContent = collapsed ? "▶" : "▼";
        });
    }

    return node;
}

function renderDoc(doc) {
    contentEl.innerHTML = "";
    placeholder.style.display = "none";

    const rootHolder = {
        get type() { return doc.parsed.rootType; },
        get value() { return doc.parsed.root; },
        set value(v) { doc.parsed.root = v; },
    };
    const rootName = doc.parsed.rootName || doc.parsed.name || doc.name;
    const root = renderNode(rootName, rootHolder, true);
    contentEl.appendChild(root);

    const rootChildren = root.querySelector(":scope > .children");
    if (rootChildren) {
        rootChildren.classList.remove("collapsed");
        const rootToggle = root.querySelector(":scope > .node-head .toggle");
        if (rootToggle && !rootToggle.classList.contains("leaf")) rootToggle.textContent = "▼";
    }
    applyFilter();

    if (doc.parsed.trailing > 0) {
        setStatus(`Loaded ${doc.name} — note: ${doc.parsed.trailing} unread trailing byte(s)`, "ok");
    } else {
        setStatus(`Loaded ${doc.name}`, "ok");
    }
}

function expandNode(node, expand) {
    const toggles = node.querySelectorAll(":scope .children");
    toggles.forEach((childContainer) => {
        childContainer.classList.toggle("collapsed", !expand);
        const head = childContainer.previousElementSibling;
        if (head) {
            const toggle = head.querySelector(".toggle");
            if (toggle && !toggle.classList.contains("leaf")) {
                toggle.textContent = expand ? "▼" : "▶";
            }
        }
    });
}

function updateSaveButton() {
    const doc = docs.find((d) => d.id === activeDocId);
    saveBtn.disabled = !doc;
    saveBtn.textContent = doc && doc.dirty ? "Save (download)*" : "Save (download)";
}

function markActiveDocDirty() {
    const doc = docs.find((d) => d.id === activeDocId);
    if (doc && !doc.dirty) {
        doc.dirty = true;
        renderTabs();
        updateSaveButton();
    } else if (doc) {
        updateSaveButton();
    }
}

function setActiveDoc(id) {
    activeDocId = id;
    renderTabs();
    const doc = docs.find((d) => d.id === id);
    if (doc) {
        renderDoc(doc);
    } else {
        contentEl.innerHTML = "";
        placeholder.style.display = "flex";
        setStatus("Idle");
    }
    updateSaveButton();
}

function renderTabs() {
    tabsEl.innerHTML = "";
    for (const doc of docs) {
        const tab = document.createElement("div");
        tab.className = "tab" + (doc.id === activeDocId ? " active" : "") + (doc.dirty ? " dirty" : "");

        const label = document.createElement("span");
        label.textContent = (doc.dirty ? "● " : "") + doc.name;
        label.title = `${doc.name} (${formatBytes(doc.size)})${doc.dirty ? " — unsaved changes" : ""}`;
        label.addEventListener("click", () => setActiveDoc(doc.id));
        tab.appendChild(label);

        const close = document.createElement("span");
        close.className = "close";
        close.textContent = "×";
        close.title = "Close";
        close.addEventListener("click", (e) => {
            e.stopPropagation();
            closeDoc(doc.id);
        });
        tab.appendChild(close);

        tabsEl.appendChild(tab);
    }
}

function closeDoc(id) {
    const idx = docs.findIndex((d) => d.id === id);
    if (idx < 0) return;
    docs.splice(idx, 1);
    if (docs.length === 0) {
        setActiveDoc(null);
    } else if (activeDocId === id) {
        setActiveDoc(docs[Math.min(idx, docs.length - 1)].id);
    } else {
        renderTabs();
    }
}

async function openFile(file) {
    try {
        setStatus(`Reading ${file.name}...`);
        const buffer = await file.arrayBuffer();
        const parsed = parseBBT(buffer);
        const doc = {
            id: ++docIdSeq,
            name: file.name,
            size: file.size,
            parsed,
            dirty: false,
        };
        docs.push(doc);
        setActiveDoc(doc.id);
    } catch (err) {
        console.error(err);
        setStatus(`${file.name}: ${err.message}`, "error");
    }
}

async function openFiles(fileList) {
    for (const file of fileList) {
        await openFile(file);
    }
}

fileInput.addEventListener("change", () => {
    if (fileInput.files.length > 0) {
        openFiles(fileInput.files);
        fileInput.value = "";
    }
});

["dragenter", "dragover"].forEach((ev) => {
    dropZone.addEventListener(ev, (e) => {
        e.preventDefault();
        e.stopPropagation();
        dropZone.classList.add("drag-over");
    });
});

["dragleave", "drop"].forEach((ev) => {
    dropZone.addEventListener(ev, (e) => {
        e.preventDefault();
        e.stopPropagation();
        if (ev === "dragleave" && e.target !== dropZone) return;
        dropZone.classList.remove("drag-over");
    });
});

dropZone.addEventListener("drop", (e) => {
    const files = e.dataTransfer?.files;
    if (files && files.length > 0) {
        openFiles(files);
    }
});

saveBtn.addEventListener("click", () => {
    const doc = docs.find((d) => d.id === activeDocId);
    if (!doc) return;
    try {
        const bytes = window.BBT.serializeBBT(doc.parsed);
        const blob = new Blob([bytes], { type: "application/octet-stream" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = doc.name;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        doc.dirty = false;
        doc.size = bytes.length;
        renderTabs();
        updateSaveButton();
        setStatus(`Saved ${doc.name} (${formatBytes(bytes.length)})`, "ok");
    } catch (err) {
        console.error(err);
        setStatus(`Save ${doc.name}: ${err.message}`, "error");
    }
});

expandAllBtn.addEventListener("click", () => {
    const root = contentEl.firstElementChild;
    if (root) expandNode(root, true);
});

collapseAllBtn.addEventListener("click", () => {
    const root = contentEl.firstElementChild;
    if (root) {
        expandNode(root, false);
        const rootChildren = root.querySelector(":scope > .children");
        if (rootChildren) {
            rootChildren.classList.remove("collapsed");
            const rootHead = root.querySelector(":scope > .node-head .toggle");
            if (rootHead && !rootHead.classList.contains("leaf")) rootHead.textContent = "▼";
        }
    }
});

function applyFilter() {
    const q = filterInput.value.trim().toLowerCase();
    const nodes = contentEl.querySelectorAll(".node");
    nodes.forEach((n) => {
        const head = n.querySelector(":scope > .node-head");
        if (head) head.classList.remove("match");
    });
    if (!q) return;

    const matches = [];
    nodes.forEach((n) => {
        const name = (n.dataset.name || "").toLowerCase();
        if (name.includes(q)) matches.push(n);
    });

    matches.forEach((n) => {
        const head = n.querySelector(":scope > .node-head");
        if (head) head.classList.add("match");
        let p = n.parentElement;
        while (p && p !== contentEl) {
            if (p.classList.contains("children")) {
                p.classList.remove("collapsed");
                const parentHead = p.previousElementSibling;
                if (parentHead) {
                    const toggle = parentHead.querySelector(".toggle");
                    if (toggle && !toggle.classList.contains("leaf")) toggle.textContent = "▼";
                }
            }
            p = p.parentElement;
        }
    });

    if (matches.length === 0) {
        setStatus(`No matches for "${q}"`);
    } else {
        setStatus(`${matches.length} match${matches.length === 1 ? "" : "es"} for "${q}"`);
        matches[0].scrollIntoView({ block: "nearest", behavior: "smooth" });
    }
}

filterInput.addEventListener("input", applyFilter);

})();
