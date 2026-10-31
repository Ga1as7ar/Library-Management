/**
 * ⌘K / Ctrl+K command palette: fuzzy-ish search across pages, books and
 * readers, fully keyboard driven (↑ ↓ Enter Esc).
 */
import { api } from "./api.js";
import { $, avatar, cover, html, highlight, icon } from "./dom.js";
import { closeAnimated, mountDialog } from "./ui.js";

const PAGES = [
  { title: "概览", sub: "统计与即将到期", glyph: "layout-dashboard", href: "#/" },
  { title: "图书", sub: "馆藏目录与库存", glyph: "books", href: "#/books" },
  { title: "读者", sub: "读者与借阅额度", glyph: "users", href: "#/students" },
  { title: "借阅", sub: "借出、归还、逾期", glyph: "arrows-exchange", href: "#/loans" },
  { title: "逾期借阅", sub: "需要催还的图书", glyph: "clock", href: "#/loans?status=overdue" },
];

function results(query, books, students) {
  const q = query.trim().toLowerCase();
  const match = (...fields) => !q || fields.some((f) => String(f).toLowerCase().includes(q));
  return [
    { group: "页面", items: PAGES.filter((p) => match(p.title, p.sub)).map((p) => ({ ...p, lead: html`<span class="stat__icon" style="position:static;width:34px;height:34px">${icon(p.glyph)}</span>` })) },
    { group: "图书", items: books.filter((b) => match(b.title, b.author, b.isbn)).slice(0, 6).map((b) => ({ title: b.title, sub: `${b.author || "佚名"} · 可借 ${b.availableCopies}/${b.totalCopies}`, href: `#/books?q=${encodeURIComponent(b.title)}`, lead: cover(b.title) })) },
    { group: "读者", items: students.filter((s) => match(s.name, s.id)).slice(0, 6).map((s) => ({ title: s.name, sub: `学号 ${s.id} · 在借 ${s.activeLoans} 本`, href: `#/loans?studentId=${s.id}`, lead: avatar(s.name) })) },
  ].filter((g) => g.items.length);
}

export async function openPalette() {
  if ($("dialog.palette")) return;
  const dialog = mountDialog(html`
    <div class="palette__input">${icon("search")}<input type="text" placeholder="搜索页面、图书或读者…" aria-label="搜索" autocomplete="off"><kbd>Esc</kbd></div>
    <div class="palette__list" role="listbox"></div>
    <div class="palette__foot">
      <span><kbd>↑</kbd><kbd>↓</kbd> 选择</span><span><kbd>↵</kbd> 打开</span><span><kbd>Esc</kbd> 关闭</span>
    </div>`, { className: "palette" });

  const input = $("input", dialog);
  const list = $(".palette__list", dialog);
  let books = [];
  let students = [];
  let flat = [];
  let active = 0;

  const select = (index) => {
    active = Math.max(0, Math.min(flat.length - 1, index));
    list.querySelectorAll(".palette__item").forEach((el, i) => el.setAttribute("aria-selected", String(i === active)));
    list.querySelector('[aria-selected="true"]')?.scrollIntoView({ block: "nearest" });
  };

  const render = () => {
    const groups = results(input.value, books, students);
    flat = groups.flatMap((g) => g.items);
    let i = 0;
    list.innerHTML = flat.length
      ? html`${groups.map((g) => html`
          <div class="palette__group">${g.group}</div>
          ${g.items.map((item) => html`
            <button class="palette__item" role="option" type="button" data-href="${item.href}" style="--i:${i++}">
              ${item.lead}
              <span class="grow"><div class="cell-title">${highlight(item.title, input.value.trim())}</div><div class="cell-sub">${item.sub}</div></span>
              ${icon("corner-down-left", "palette__go")}
            </button>`)}`)}`
      : html`<div class="palette__empty">没有找到「${input.value}」</div>`;
    select(0);
  };

  const go = (href) => {
    closeAnimated(dialog);
    if (href) location.hash = href.slice(1);
  };

  input.addEventListener("input", render);
  input.addEventListener("keydown", (e) => {
    if (e.key === "ArrowDown") { e.preventDefault(); select(active + 1); }
    if (e.key === "ArrowUp") { e.preventDefault(); select(active - 1); }
    if (e.key === "Enter") { e.preventDefault(); go(flat[active]?.href); }
  });
  list.addEventListener("click", (e) => go(e.target.closest(".palette__item")?.dataset.href));
  list.addEventListener("pointermove", (e) => {
    const item = e.target.closest(".palette__item");
    if (item) select([...list.querySelectorAll(".palette__item")].indexOf(item));
  });

  render();
  input.focus();
  [books, students] = await Promise.all([api.books.list(), api.students.list()]).catch(() => [[], []]);
  if (dialog.isConnected) render();
}
