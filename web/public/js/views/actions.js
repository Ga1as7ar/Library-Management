/**
 * Cross-view workflows (borrow / return). Each returns true when data changed,
 * so the calling view knows to refresh. A `library:changed` event is also
 * dispatched so the shell can refresh global badges.
 */
import { api } from "../api.js";
import { formatDate, html } from "../dom.js";
import { confirmDialog, field, formDialog, selectField, toast } from "../ui.js";

const changed = () => window.dispatchEvent(new Event("library:changed"));

/** Borrow dialog. Pre-selects `bookId` / `studentId` when launched from a row. */
export async function borrowFlow({ bookId, studentId } = {}) {
  const [books, students] = await Promise.all([api.books.list(), api.students.list()]);
  if (!students.length) {
    toast("请先添加读者", "info");
    return false;
  }

  const loan = await formDialog({
    title: "办理借阅",
    glyph: "book-upload",
    submitLabel: "确认借出",
    body: html`
      ${selectField({
        label: "读者",
        name: "studentId",
        required: true,
        value: studentId ?? "",
        options: [
          { value: "", label: "请选择读者", disabled: true },
          ...students.map((s) => ({ value: s.id, label: `${s.name}（${s.id}）· 在借 ${s.activeLoans} 本` })),
        ],
      })}
      ${selectField({
        label: "图书",
        name: "bookId",
        required: true,
        value: bookId ?? "",
        options: [
          { value: "", label: "请选择图书", disabled: true },
          ...books.map((b) => ({
            value: b.id,
            label: `${b.title}（可借 ${b.availableCopies}/${b.totalCopies}）`,
            disabled: b.availableCopies === 0,
          })),
        ],
      })}
      ${field({ label: "借阅天数", name: "days", type: "number", value: 30, required: true, attrs: 'min="1" max="180"', hint: "1–180 天，逾期未还将无法继续借书" })}`,
    onSubmit: (values) =>
      api.loans.borrow({ studentId: Number(values.studentId), bookId: Number(values.bookId), days: Number(values.days) }),
  });

  if (!loan) return false;
  toast(`《${loan.bookTitle}》已借给 ${loan.studentName}，应还日期 ${formatDate(loan.dueAt)}`);
  changed();
  return true;
}

export async function returnFlow(loan) {
  const ok = await confirmDialog({
    title: "确认归还",
    message: `${loan.studentName} 归还《${loan.bookTitle}》？`,
    confirmLabel: "确认归还",
    danger: false,
  });
  if (!ok) return false;
  try {
    await api.loans.giveBack(loan.id);
    toast(`《${loan.bookTitle}》已归还`);
    changed();
    return true;
  } catch (error) {
    toast(error.message, "error");
    return false;
  }
}

export { changed as notifyChanged };
