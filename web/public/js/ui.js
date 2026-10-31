/**
 * Reusable UI pieces: toasts, modal forms, confirmation dialogs and
 * loading / empty / error states.
 */
import { $, fragment, html, icon, raw } from "./dom.js";
import { reducedMotion } from "./motion.js";

// ------------------------------------------------------------------ toasts

const TOAST_ICONS = { success: "circle-check", error: "alert-triangle", info: "info-circle" };

export function toast(message, type = "success") {
  const node = fragment(html`<div class="toast toast--${type}">${icon(TOAST_ICONS[type])}<span>${message}</span></div>`).firstChild;
  $("#toasts").append(node);
  setTimeout(() => {
    node.classList.add("is-leaving");
    node.addEventListener("animationend", () => node.remove(), { once: true });
  }, 3200);
}

// ------------------------------------------------------------------ modals

/** Closes a <dialog> after its exit animation has played. */
export function closeAnimated(dialog) {
  if (!dialog.open || dialog.classList.contains("is-closing")) return;
  if (reducedMotion()) return dialog.close();
  dialog.classList.add("is-closing");
  dialog.addEventListener("animationend", () => {
    dialog.classList.remove("is-closing");
    dialog.close();
  }, { once: true });
}

export function mountDialog(markup, { className = "modal", size } = {}) {
  const dialog = document.createElement("dialog");
  dialog.className = `${className}${size ? ` ${className}--${size}` : ""}`;
  dialog.append(fragment(markup));
  document.body.append(dialog);
  dialog.addEventListener("close", () => dialog.remove());
  dialog.addEventListener("cancel", (e) => {  // Esc: animate instead of instant close
    e.preventDefault();
    closeAnimated(dialog);
  });
  dialog.addEventListener("click", (e) => {
    if (e.target === dialog) closeAnimated(dialog);  // click on backdrop
  });
  dialog.showModal();
  return dialog;
}

/**
 * Opens a form dialog. `onSubmit(values)` may return a promise; if it rejects,
 * the error is shown inline (with a shake) and the dialog stays open.
 * Resolves with onSubmit's result, or undefined when cancelled.
 */
export function formDialog({ title, glyph = "pencil", body, submitLabel = "保存", onSubmit, onMount }) {
  return new Promise((resolve) => {
    const dialog = mountDialog(html`
      <form method="dialog" novalidate>
        <div class="modal__head">
          <div class="modal__title"><span class="badge-icon">${icon(glyph)}</span><h2>${title}</h2></div>
          <button class="btn btn--ghost btn--icon" type="button" data-close aria-label="关闭">${icon("x")}</button>
        </div>
        <div class="modal__body">
          ${body}
          <div class="error-box modal__error" hidden>${icon("alert-triangle")}<span></span></div>
        </div>
        <div class="modal__foot">
          <button class="btn" type="button" data-close>取消</button>
          <button class="btn btn--primary" type="submit">${submitLabel}</button>
        </div>
      </form>`);

    const form = $("form", dialog);
    const errorBox = $(".modal__error", dialog);
    const submit = $('[type="submit"]', dialog);
    let result;

    dialog.querySelectorAll("[data-close]").forEach((b) => b.addEventListener("click", () => closeAnimated(dialog)));
    dialog.addEventListener("close", () => resolve(result));
    $("input:not([readonly]), select", dialog)?.focus();
    onMount?.(dialog);

    const showError = (message) => {
      $("span", errorBox).textContent = message;
      errorBox.hidden = false;
      errorBox.classList.remove("shake");
      void errorBox.offsetWidth;  // restart the animation
      errorBox.classList.add("shake");
    };

    form.addEventListener("submit", async (event) => {
      event.preventDefault();
      if (!form.checkValidity()) {
        const invalid = form.querySelector(":invalid");
        showError(`${invalid.closest(".field")?.querySelector(".field__label")?.firstChild?.textContent?.trim() ?? "表单"}：${invalid.validationMessage}`);
        invalid.focus();
        return;
      }
      submit.disabled = true;
      try {
        result = await onSubmit(Object.fromEntries(new FormData(form)), form);
        closeAnimated(dialog);
      } catch (error) {
        showError(error.message);
      } finally {
        submit.disabled = false;
      }
    });
  });
}

/** Resolves true when the user confirms. */
export function confirmDialog({ title, message, confirmLabel = "删除", danger = true }) {
  return new Promise((resolve) => {
    const dialog = mountDialog(
      html`
        <div class="modal__body" style="grid-template-columns:auto 1fr;align-items:start;gap:16px">
          <span class="confirm-icon ${danger ? "" : "confirm-icon--accent"}">${icon(danger ? "alert-triangle" : "book-download")}</span>
          <div><h2 style="font-size:16px;font-weight:650">${title}</h2><p class="muted" style="margin-top:4px">${message}</p></div>
        </div>
        <div class="modal__foot">
          <button class="btn" type="button" data-answer="no">取消</button>
          <button class="btn ${danger ? "btn--danger" : "btn--primary"}" type="button" data-answer="yes">${confirmLabel}</button>
        </div>`,
      { size: "sm" },
    );
    let answer = false;
    dialog.querySelectorAll("[data-answer]").forEach((button) =>
      button.addEventListener("click", () => {
        answer = button.dataset.answer === "yes";
        closeAnimated(dialog);
      }),
    );
    dialog.addEventListener("close", () => resolve(answer));
    $(`[data-answer="${danger ? "no" : "yes"}"]`, dialog).focus();
  });
}

// ------------------------------------------------------------------ fields

export function field({ label, name, value = "", type = "text", required = false, hint, attrs = "", span = false }) {
  const id = `f-${name}`;
  return html`
    <label class="field ${span ? "span-2" : ""}" for="${id}">
      <span class="field__label">${label}${required ? html`<span class="req">*</span>` : ""}</span>
      <input class="input" id="${id}" name="${name}" type="${type}" value="${value}" ${required ? "required" : ""} ${raw(attrs)}>
      ${hint ? html`<span class="field__hint">${hint}</span>` : ""}
    </label>`;
}

export function selectField({ label, name, options, value, required = false, span = false }) {
  const id = `f-${name}`;
  return html`
    <label class="field ${span ? "span-2" : ""}" for="${id}">
      <span class="field__label">${label}${required ? html`<span class="req">*</span>` : ""}</span>
      <select class="select" id="${id}" name="${name}" ${required ? "required" : ""}>
        ${options.map((o) => html`<option value="${o.value}" ${String(o.value) === String(value) ? "selected" : ""} ${o.disabled ? "disabled" : ""}>${o.label}</option>`)}
      </select>
    </label>`;
}

// ------------------------------------------------------------------ states

export const skeletonRows = (columns, rows = 5) =>
  html`${Array.from({ length: rows }, () => html`<tr>${Array.from({ length: columns }, () => html`<td><span class="skeleton"></span></td>`)}</tr>`)}`;

export const emptyState = ({ glyph = "inbox", title, text, action }) =>
  html`<div class="empty"><span class="empty__art">${icon(glyph)}</span><strong>${title}</strong><span>${text}</span>${action ?? ""}</div>`;

export const errorState = (error) =>
  html`<div class="error-box">${icon("alert-triangle")}<span>${error.message}</span></div>`;

