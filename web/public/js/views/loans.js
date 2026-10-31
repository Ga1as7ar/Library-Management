import { api } from "../api.js";
import { avatar, cover, daysUntil, formatDate, html, icon } from "../dom.js";
import { animateSegmented, leave, stagger } from "../motion.js";
import { emptyState, errorState, skeletonRows } from "../ui.js";
import { borrowFlow, returnFlow } from "./actions.js";

const COLUMNS = 5;
const FILTERS = [
  { value: "active", label: "在借" },
  { value: "overdue", label: "逾期" },
  { value: "returned", label: "已归还" },
  { value: "", label: "全部" },
];

function statusBadge(loan) {
  if (loan.returnedAt) return html`<span class="badge">已归还 · ${formatDate(loan.returnedAt)}</span>`;
  const days = daysUntil(loan.dueAt);
  if (days < 0) return html`<span class="badge badge--danger">逾期 ${-days} 天</span>`;
  if (days <= 3) return html`<span class="badge badge--warning">剩 ${days} 天</span>`;
  return html`<span class="badge badge--accent">剩 ${days} 天</span>`;
}

/** Thin progress line: how much of the loan period has elapsed. */
function periodCell(loan) {
  const end = loan.returnedAt ?? Math.floor(Date.now() / 1000);
  const elapsed = Math.min(1, Math.max(0, (end - loan.borrowedAt) / (loan.dueAt - loan.borrowedAt)));
  const over = !loan.returnedAt && end > loan.dueAt;
  return html`
    <div class="num" style="font-size:13px">${formatDate(loan.borrowedAt)} → ${formatDate(loan.dueAt)}</div>
    <div class="stock__bar" style="margin-top:6px;max-width:200px;height:4px">
      <div class="stock__fill ${over ? "is-empty" : ""}" style="width:${elapsed * 100}%;${loan.returnedAt ? "background:var(--border-strong)" : ""}"></div>
    </div>`;
}

function row(loan) {
  return html`
    <tr class="enter" data-row="${loan.id}">
      <td><div class="cell-main">${cover(loan.bookTitle)}<div><div class="cell-title">${loan.bookTitle}</div><div class="cell-sub">借阅号 #${loan.id}</div></div></div></td>
      <td><div class="cell-main">${avatar(loan.studentName)}<div><div class="nowrap">${loan.studentName}</div><div class="cell-sub mono">${loan.studentId}</div></div></div></td>
      <td class="hide-sm">${periodCell(loan)}</td>
      <td>${statusBadge(loan)}</td>
      <td class="actions">${loan.returnedAt ? "" : html`<button class="btn btn--sm" data-return="${loan.id}">${icon("book-download")}归还</button>`}</td>
    </tr>`;
}

/** @param params URLSearchParams from the hash route, e.g. #/loans?studentId=1&status=overdue */
export function loansView(root, params) {
  const studentId = params.get("studentId") ?? undefined;
  const initialStatus = params.get("status") ?? (studentId ? "" : "active");
  const state = { status: initialStatus, studentId, loans: [] };

  root.innerHTML = html`
    <header class="page-head enter">
      <div><h1 data-title>借阅</h1><p data-subtitle>借出、归还与逾期跟踪</p></div>
      <button class="btn btn--primary" data-action="borrow">${icon("book-upload")}办理借阅</button>
    </header>
    <div class="toolbar enter">
      <div class="segmented" role="group" aria-label="状态筛选">
        ${FILTERS.map((f) => html`<button type="button" data-status="${f.value}" aria-pressed="${f.value === state.status}">${f.label}</button>`)}
      </div>
    </div>
    <div class="panel enter"><div class="table-wrap"><table class="table">
      <thead><tr><th>图书</th><th>读者</th><th class="hide-sm">借阅期间</th><th>状态</th><th></th></tr></thead>
      <tbody>${skeletonRows(COLUMNS)}</tbody>
    </table></div></div>`;

  const tbody = root.querySelector("tbody");
  const segmented = root.querySelector(".segmented");
  const syncThumb = animateSegmented(segmented);
  let student = null;

  async function refresh() {
    try {
      state.loans = await api.loans.list(state);
      tbody.innerHTML = state.loans.length
        ? html`${state.loans.map(row)}`
        : html`<tr><td colspan="${COLUMNS}">${emptyState({
            glyph: state.status === "overdue" ? "circle-check" : "arrows-exchange",
            title: state.status === "overdue" ? "没有逾期借阅" : "暂无借阅记录",
            text: state.status === "overdue" ? "所有在借图书都在期限内" : "通过「办理借阅」借出第一本书",
          })}</td></tr>`;
      stagger(tbody);
    } catch (error) {
      tbody.innerHTML = html`<tr><td colspan="${COLUMNS}">${errorState(error)}</td></tr>`;
    }
  }

  segmented.addEventListener("click", (e) => {
    const button = e.target.closest("button");
    if (!button) return;
    segmented.querySelectorAll("button").forEach((b) => b.setAttribute("aria-pressed", String(b === button)));
    syncThumb();
    state.status = button.dataset.status;
    refresh();
  });
  root.addEventListener("click", async (e) => {
    const target = e.target.closest("[data-action], [data-return]");
    if (!target) return;
    if (target.dataset.return) {
      if (await returnFlow(state.loans.find((l) => l.id === Number(target.dataset.return)))) {
        if (state.status === "active" || state.status === "overdue") await leave(target.closest("tr"));
        refresh();
      }
    } else if (await borrowFlow({ studentId: student?.id })) {
      refresh();
    }
  });

  const loadHeader = async () => {
    if (!studentId) return;
    student = await api.students.get(studentId).catch(() => null);
    if (!student) return;
    root.querySelector("[data-title]").textContent = `${student.name} 的借阅`;
    root.querySelector("[data-subtitle]").innerHTML = html`<a class="muted" href="#/loans">← 查看全部借阅</a>`;
  };

  return Promise.all([loadHeader(), refresh()]);
}
