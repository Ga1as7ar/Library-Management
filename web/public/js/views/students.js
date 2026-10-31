import { api } from "../api.js";
import { avatar, debounce, highlight, html, icon } from "../dom.js";
import { leave, stagger } from "../motion.js";
import { confirmDialog, emptyState, errorState, field, formDialog, skeletonRows, toast } from "../ui.js";
import { borrowFlow, notifyChanged } from "./actions.js";

const COLUMNS = 4;
const MAX_LOANS = 5;  // mirrors LibraryService::kMaxActiveLoansPerStudent

/** Five little slots that fill up as the reader borrows more. */
function quota(active) {
  const tone = active >= MAX_LOANS ? "var(--warning)" : "var(--accent)";
  return html`
    <div style="display:flex;align-items:center;gap:10px">
      <div style="display:flex;gap:3px" aria-hidden="true">
        ${Array.from({ length: MAX_LOANS }, (_, i) => html`<span style="width:14px;height:6px;border-radius:3px;background:${i < active ? tone : "var(--surface-hover)"};transition:background .3s"></span>`)}
      </div>
      <span class="cell-sub num" style="margin:0">${active} / ${MAX_LOANS}</span>
    </div>`;
}

function row(student, q) {
  const full = student.activeLoans >= MAX_LOANS;
  return html`
    <tr class="enter" data-row="${student.id}">
      <td>
        <div class="cell-main">
          ${avatar(student.name)}
          <div><div class="cell-title">${highlight(student.name, q)}</div><div class="cell-sub">${student.email || "未填写邮箱"}</div></div>
        </div>
      </td>
      <td class="mono">${highlight(String(student.id), q)}</td>
      <td>${quota(student.activeLoans)}</td>
      <td class="actions">
        <a class="btn btn--sm btn--ghost hide-sm reveal" href="#/loans?studentId=${student.id}">借阅记录</a>
        <button class="btn btn--sm" data-act="borrow" data-id="${student.id}" ${full ? "disabled" : ""}>借书</button>
        <button class="btn btn--ghost btn--icon btn--sm reveal" data-act="edit" data-id="${student.id}" aria-label="编辑" title="编辑">${icon("pencil")}</button>
        <button class="btn btn--ghost btn--icon btn--sm is-danger reveal" data-act="delete" data-id="${student.id}" aria-label="删除" title="删除">${icon("trash")}</button>
      </td>
    </tr>`;
}

export function studentsView(root) {
  const state = { q: "", students: [] };

  root.innerHTML = html`
    <header class="page-head enter">
      <div><h1>读者</h1><p>每位读者最多同时借阅 ${MAX_LOANS} 本，逾期未还时暂停借阅</p></div>
      <button class="btn btn--primary" data-act="create">${icon("plus")}新增读者</button>
    </header>
    <div class="toolbar enter">
      <label class="search">${icon("search")}<input class="input" type="search" placeholder="搜索姓名或学号" aria-label="搜索读者"></label>
    </div>
    <div class="panel enter"><div class="table-wrap"><table class="table">
      <thead><tr><th>读者</th><th>学号</th><th>借阅额度</th><th></th></tr></thead>
      <tbody>${skeletonRows(COLUMNS)}</tbody>
    </table></div></div>`;

  const tbody = root.querySelector("tbody");

  async function refresh() {
    try {
      state.students = await api.students.list(state);
      const q = state.q.trim();
      tbody.innerHTML = state.students.length
        ? html`${state.students.map((s) => row(s, q))}`
        : html`<tr><td colspan="${COLUMNS}">${emptyState({ glyph: "school", title: state.q ? "没有匹配的读者" : "还没有读者", text: state.q ? "换个关键词试试" : "添加读者后即可办理借阅" })}</td></tr>`;
      stagger(tbody);
    } catch (error) {
      tbody.innerHTML = html`<tr><td colspan="${COLUMNS}">${errorState(error)}</td></tr>`;
    }
  }

  async function edit(student) {
    const saved = await formDialog({
      title: student ? "编辑读者" : "新增读者",
      glyph: student ? "pencil" : "users",
      body: html`
        ${field({ label: "学号", name: "id", type: "number", value: student?.id ?? "", required: true, attrs: `min="1" ${student ? "readonly" : ""}`, hint: student ? "学号创建后不可修改" : "" })}
        ${field({ label: "姓名", name: "name", value: student?.name ?? "", required: true, attrs: 'maxlength="100"' })}
        ${field({ label: "邮箱", name: "email", type: "email", value: student?.email ?? "", hint: "选填，用于到期提醒" })}`,
      onSubmit: (v) => {
        const payload = { id: Number(v.id), name: v.name, email: v.email };
        return student ? api.students.update(student.id, payload) : api.students.create(payload);
      },
    });
    if (saved) {
      toast(student ? `已更新 ${saved.name}` : `已添加读者 ${saved.name}`);
      notifyChanged();
      refresh();
    }
  }

  async function remove(student) {
    if (!(await confirmDialog({ title: "删除读者", message: `确定删除 ${student.name}（${student.id}）？其历史借阅记录也会一并删除。` }))) return;
    try {
      await api.students.remove(student.id);
      toast(`已删除 ${student.name}`);
      notifyChanged();
      await leave(tbody.querySelector(`[data-row="${student.id}"]`));
      refresh();
    } catch (error) {
      toast(error.message, "error");
    }
  }

  root.querySelector('input[type="search"]').addEventListener("input", debounce((e) => {
    state.q = e.target.value;
    refresh();
  }));
  root.addEventListener("click", async (e) => {
    const button = e.target.closest("[data-act]");
    if (!button) return;
    const student = state.students.find((s) => s.id === Number(button.dataset.id));
    switch (button.dataset.act) {
      case "create": return edit();
      case "edit": return edit(student);
      case "delete": return remove(student);
      case "borrow": if (await borrowFlow({ studentId: student.id })) refresh(); return;
    }
  });
  tbody.addEventListener("dblclick", (e) => {
    const id = Number(e.target.closest("[data-row]")?.dataset.row);
    if (id && !e.target.closest("button, a")) edit(state.students.find((s) => s.id === id));
  });

  return refresh();
}
