import { api } from "../api.js";
import { cover, debounce, formatPrice, highlight, html, icon } from "../dom.js";
import { animateSegmented, leave, stagger } from "../motion.js";
import { confirmDialog, emptyState, errorState, field, formDialog, selectField, skeletonRows, toast } from "../ui.js";
import { borrowFlow, notifyChanged } from "./actions.js";

const COLUMNS = 6;

function stockCell(book, index) {
  const pct = book.totalCopies ? (book.availableCopies / book.totalCopies) * 100 : 0;
  return html`
    <div class="stock" title="可借 ${book.availableCopies} / 共 ${book.totalCopies}">
      <div class="stock__bar"><div class="stock__fill ${book.availableCopies ? "" : "is-empty"}" style="width:${book.availableCopies ? pct : 100}%;--i:${index}"></div></div>
      <span class="stock__text num">${book.availableCopies}/${book.totalCopies}</span>
    </div>`;
}

function row(book, index, q) {
  const tag = book.category === "textbook"
    ? html`<span class="badge badge--accent">教材</span>`
    : html`<span class="badge">普通</span>`;
  const detail = book.category === "textbook" && book.course ? ` · ${book.course}` : "";
  return html`
    <tr class="enter" data-row="${book.id}">
      <td>
        <div class="cell-main">
          ${cover(book.title)}
          <div style="min-width:0">
            <div class="cell-title">${highlight(book.title, q)}</div>
            <div class="cell-sub">${highlight(book.author || "佚名", q)}${book.publisher ? ` · ${book.publisher}` : ""}${detail}</div>
          </div>
        </div>
      </td>
      <td class="hide-sm">${tag}</td>
      <td class="mono hide-sm">${highlight(book.isbn, q)}</td>
      <td class="right num hide-sm">${formatPrice(book.priceCents)}</td>
      <td>${stockCell(book, index)}</td>
      <td class="actions">
        <button class="btn btn--sm" data-act="borrow" data-id="${book.id}" ${book.availableCopies ? "" : "disabled"}>借出</button>
        <button class="btn btn--ghost btn--icon btn--sm reveal" data-act="edit" data-id="${book.id}" aria-label="编辑" title="编辑">${icon("pencil")}</button>
        <button class="btn btn--ghost btn--icon btn--sm is-danger reveal" data-act="delete" data-id="${book.id}" aria-label="删除" title="删除">${icon("trash")}</button>
      </td>
    </tr>`;
}

function bookForm(book = {}) {
  const isTextbook = book.category === "textbook";
  return html`
    <div class="form-grid">
      ${field({ label: "书名", name: "title", value: book.title ?? "", required: true, span: true, attrs: 'maxlength="200"' })}
      ${field({ label: "作者", name: "author", value: book.author ?? "" })}
      ${field({ label: "出版社", name: "publisher", value: book.publisher ?? "" })}
      ${field({ label: "ISBN", name: "isbn", value: book.isbn ?? "", required: true, hint: "10 或 13 位，可含连字符" })}
      ${field({ label: "价格（元）", name: "price", type: "number", value: ((book.priceCents ?? 0) / 100).toFixed(2), attrs: 'min="0" step="0.01"' })}
      ${field({ label: "馆藏数量", name: "totalCopies", type: "number", value: book.totalCopies ?? 1, required: true, attrs: 'min="0" max="9999"' })}
      ${selectField({ label: "类别", name: "category", value: book.category ?? "general", options: [{ value: "general", label: "普通图书" }, { value: "textbook", label: "教材" }] })}
      <div class="form-grid span-2" data-textbook ${isTextbook ? "" : "hidden"}>
        ${field({ label: "学科", name: "subject", value: book.subject ?? "" })}
        ${field({ label: "课程", name: "course", value: book.course ?? "" })}
      </div>
    </div>`;
}

const toPayload = (v) => ({
  title: v.title,
  author: v.author,
  publisher: v.publisher,
  isbn: v.isbn,
  priceCents: Math.round(Number(v.price || 0) * 100),
  totalCopies: Number(v.totalCopies),
  category: v.category,
  subject: v.subject ?? "",
  course: v.course ?? "",
});

/** Shows the textbook-only fields when the category select says so. */
function wireCategoryToggle(dialog) {
  const select = dialog.querySelector('[name="category"]');
  const group = dialog.querySelector("[data-textbook]");
  select.addEventListener("change", () => {
    group.hidden = select.value !== "textbook";
    if (!group.hidden) group.querySelectorAll(".field").forEach((f, i) => {
      f.classList.add("enter");
      f.style.setProperty("--i", i);
    });
  });
}

export function booksView(root, params) {
  const state = { q: params.get("q") ?? "", category: "", available: false, books: [] };

  root.innerHTML = html`
    <header class="page-head enter">
      <div><h1>图书</h1><p>管理馆藏目录与库存 · 按 <kbd>/</kbd> 搜索</p></div>
      <button class="btn btn--primary" data-act="create">${icon("plus")}新增图书</button>
    </header>
    <div class="toolbar enter">
      <label class="search">${icon("search")}<input class="input" type="search" placeholder="搜索书名、作者、ISBN、出版社" aria-label="搜索图书" value="${state.q}"></label>
      <div class="segmented" role="group" aria-label="类别筛选">
        <button type="button" data-category="" aria-pressed="true">全部</button>
        <button type="button" data-category="general" aria-pressed="false">普通</button>
        <button type="button" data-category="textbook" aria-pressed="false">教材</button>
      </div>
      <label class="check"><input type="checkbox" data-available>仅显示可借</label>
    </div>
    <div class="panel enter"><div class="table-wrap"><table class="table">
      <thead><tr><th>书名</th><th class="hide-sm">类别</th><th class="hide-sm">ISBN</th><th class="right hide-sm">价格</th><th>库存</th><th></th></tr></thead>
      <tbody>${skeletonRows(COLUMNS)}</tbody>
    </table></div></div>`;

  const tbody = root.querySelector("tbody");
  const segmented = root.querySelector(".segmented");
  const syncThumb = animateSegmented(segmented);

  async function refresh() {
    try {
      state.books = await api.books.list(state);
      const q = state.q.trim();
      tbody.innerHTML = state.books.length
        ? html`${state.books.map((b, i) => row(b, i, q))}`
        : html`<tr><td colspan="${COLUMNS}">${emptyState({
            glyph: "books",
            title: state.q || state.category || state.available ? "没有匹配的图书" : "馆藏为空",
            text: state.q ? "换个关键词试试" : "点击右上角「新增图书」开始建立馆藏",
          })}</td></tr>`;
      stagger(tbody);
    } catch (error) {
      tbody.innerHTML = html`<tr><td colspan="${COLUMNS}">${errorState(error)}</td></tr>`;
    }
  }

  async function edit(book) {
    const saved = await formDialog({
      title: book ? "编辑图书" : "新增图书",
      glyph: book ? "pencil" : "plus",
      body: bookForm(book),
      onMount: wireCategoryToggle,
      onSubmit: (values) => (book ? api.books.update(book.id, toPayload(values)) : api.books.create(toPayload(values))),
    });
    if (saved) {
      toast(book ? `已更新《${saved.title}》` : `已添加《${saved.title}》`);
      notifyChanged();
      refresh();
    }
  }

  async function remove(book) {
    const ok = await confirmDialog({ title: "删除图书", message: `确定删除《${book.title}》？该书的历史借阅记录也会一并删除。` });
    if (!ok) return;
    try {
      await api.books.remove(book.id);
      toast(`已删除《${book.title}》`);
      notifyChanged();
      await leave(tbody.querySelector(`[data-row="${book.id}"]`));
      refresh();
    } catch (error) {
      toast(error.message, "error");
    }
  }

  root.querySelector('input[type="search"]').addEventListener("input", debounce((e) => {
    state.q = e.target.value;
    refresh();
  }));
  root.querySelector("[data-available]").addEventListener("change", (e) => {
    state.available = e.target.checked;
    refresh();
  });
  segmented.addEventListener("click", (e) => {
    const button = e.target.closest("button");
    if (!button) return;
    segmented.querySelectorAll("button").forEach((b) => b.setAttribute("aria-pressed", String(b === button)));
    syncThumb();
    state.category = button.dataset.category;
    refresh();
  });
  root.addEventListener("click", async (e) => {
    const button = e.target.closest("[data-act]");
    if (!button) return;
    const book = state.books.find((b) => b.id === Number(button.dataset.id));
    switch (button.dataset.act) {
      case "create": return edit();
      case "edit": return edit(book);
      case "delete": return remove(book);
      case "borrow": if (await borrowFlow({ bookId: book.id })) refresh(); return;
    }
  });
  tbody.addEventListener("dblclick", (e) => {
    const id = Number(e.target.closest("[data-row]")?.dataset.row);
    if (id && !e.target.closest("button")) edit(state.books.find((b) => b.id === id));
  });

  return refresh();
}
