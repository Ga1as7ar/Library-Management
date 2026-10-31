/**
 * Tiny DOM toolkit: a safe tagged-template renderer and formatting helpers.
 *
 * `html` escapes every interpolated value unless it is itself an `html`
 * fragment (or an array of fragments), so user data can never inject markup.
 */

const RAW = Symbol("raw-html");

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };
const escape = (value) => String(value).replace(/[&<>"']/g, (c) => ESCAPES[c]);

function stringify(value) {
  if (value === null || value === undefined || value === false) return "";
  if (Array.isArray(value)) return value.map(stringify).join("");
  if (value[RAW]) return value.toString();
  return escape(value);
}

export function html(strings, ...values) {
  const text = strings.reduce((out, str, i) => out + str + (i < values.length ? stringify(values[i]) : ""), "");
  return { [RAW]: true, toString: () => text };
}

/** Marks a trusted string as markup. Only for constants authored in this codebase. */
export const raw = (text) => ({ [RAW]: true, toString: () => text });

export const icon = (name, cls = "") => raw(`<svg class="icon ${cls}" aria-hidden="true"><use href="icons.svg#i-${name}"/></svg>`);

/** Parses an html fragment into a DocumentFragment. */
export function fragment(markup) {
  const template = document.createElement("template");
  template.innerHTML = markup.toString().trim();
  return template.content;
}

export const $ = (selector, root = document) => root.querySelector(selector);

// ------------------------------------------------------------------ identity

/** Stable hue (0–359) derived from a string, used for covers and avatars. */
export function hueOf(text) {
  let hash = 0;
  for (const ch of String(text)) hash = (hash * 31 + ch.codePointAt(0)) >>> 0;
  return hash % 360;
}

/** First visible character, upper-cased (works for CJK and Latin). */
export const initial = (text) => [...String(text).trim()][0]?.toUpperCase() ?? "?";

/** Generated book cover: a coloured spine with the title's first character. */
export const cover = (title, size = "") =>
  html`<span class="cover ${size ? `cover--${size}` : ""}" style="--h:${hueOf(title)}" aria-hidden="true">${initial(title)}</span>`;

export const avatar = (name) =>
  html`<span class="avatar" style="--h:${hueOf(name)}" aria-hidden="true">${[...name].slice(-2).join("")}</span>`;

/** Wraps case-insensitive matches of `query` in <mark>, escaping everything else. */
export function highlight(text, query) {
  const source = String(text);
  if (!query) return html`${source}`;
  const lower = source.toLowerCase();
  const needle = query.toLowerCase();
  const parts = [];
  let from = 0;
  for (let at = lower.indexOf(needle); at !== -1; at = lower.indexOf(needle, from)) {
    parts.push(html`${source.slice(from, at)}<mark>${source.slice(at, at + needle.length)}</mark>`);
    from = at + needle.length;
  }
  parts.push(html`${source.slice(from)}`);
  return html`${parts}`;
}

// ------------------------------------------------------------------ format

const dateFormat = new Intl.DateTimeFormat("zh-CN", { year: "numeric", month: "2-digit", day: "2-digit" });
const shortDate = new Intl.DateTimeFormat("zh-CN", { month: "numeric", day: "numeric" });
const priceFormat = new Intl.NumberFormat("zh-CN", { style: "currency", currency: "CNY" });

export const formatDate = (seconds) => dateFormat.format(new Date(seconds * 1000));
export const formatShortDate = (date) => shortDate.format(date);
export const formatPrice = (cents) => priceFormat.format(cents / 100);
export const nowSeconds = () => Math.floor(Date.now() / 1000);

/** Whole days from now until `seconds` (negative when in the past). */
export const daysUntil = (seconds) => Math.ceil((seconds - nowSeconds()) / 86400);

export function greeting(date = new Date()) {
  const hour = date.getHours();
  if (hour < 6) return "夜深了";
  if (hour < 11) return "早上好";
  if (hour < 14) return "中午好";
  if (hour < 18) return "下午好";
  return "晚上好";
}

export function debounce(fn, wait = 220) {
  let timer;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), wait);
  };
}
