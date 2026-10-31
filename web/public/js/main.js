/**
 * App shell: hash router, navigation indicator, theme switch and global shortcuts.
 *
 * Routing: #/path?query → view(root, URLSearchParams). Each navigation gets a
 * fresh <section>, so views attach listeners without cleanup bookkeeping.
 */
import { api } from "./api.js";
import { $ } from "./dom.js";
import { enableRipples, slideIndicator, stagger, transition } from "./motion.js";
import { openPalette } from "./palette.js";
import { booksView } from "./views/books.js";
import { dashboardView } from "./views/dashboard.js";
import { loansView } from "./views/loans.js";
import { studentsView } from "./views/students.js";

const routes = {
  "/": { view: dashboardView, title: "概览" },
  "/books": { view: booksView, title: "图书" },
  "/students": { view: studentsView, title: "读者" },
  "/loans": { view: loansView, title: "借阅" },
};

function parseHash() {
  const [path, search = ""] = location.hash.replace(/^#/, "").split("?");
  return { path: routes[path] ? path : "/", params: new URLSearchParams(search) };
}

function updateNav(path) {
  let active = null;
  document.querySelectorAll(".nav__item").forEach((link) => {
    const isActive = link.dataset.route === path;
    if (isActive) {
      link.setAttribute("aria-current", "page");
      active = link;
    } else {
      link.removeAttribute("aria-current");
    }
  });
  slideIndicator($(".nav__indicator"), active);
}

/** Keeps the red overdue counter on the "借阅" nav item current. */
export async function refreshBadges() {
  const badge = $("#overdue-count");
  try {
    const { overdueLoans } = await api.stats();
    badge.textContent = overdueLoans;
    badge.hidden = overdueLoans === 0;
    badge.title = `${overdueLoans} 本逾期`;
  } catch {
    badge.hidden = true;
  }
}

async function navigate() {
  const { path, params } = parseHash();
  const { view, title } = routes[path];
  document.title = `${title} · 书阁`;
  document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
  updateNav(path);

  const section = document.createElement("section");
  const rendered = view(section, params);  // views render their skeleton synchronously
  await transition(() => $("#view").replaceChildren(section));
  await rendered;
  stagger(section);
  refreshBadges();
}

function setupThemeToggle() {
  const toggle = $("#theme-toggle");
  toggle.addEventListener("click", (event) => {
    const next = document.documentElement.dataset.theme === "dark" ? "light" : "dark";
    // The new theme reveals as a circle growing from the click point.
    document.documentElement.style.setProperty("--reveal-x", `${event.clientX}px`);
    document.documentElement.style.setProperty("--reveal-y", `${event.clientY}px`);
    const apply = () => {
      document.documentElement.dataset.theme = next;
      localStorage.setItem("theme", next);
    };
    if (document.startViewTransition && !matchMedia("(prefers-reduced-motion: reduce)").matches) {
      $(".main").style.viewTransitionName = "none";  // only the root participates
      document.startViewTransition(apply).finished.finally(() => ($(".main").style.viewTransitionName = ""));
    } else {
      apply();
    }
  });
}

function setupShortcuts() {
  const isMac = /Mac|iPhone|iPad/.test(navigator.platform);
  $("#palette-key").textContent = isMac ? "⌘K" : "Ctrl K";
  $("#open-palette").addEventListener("click", openPalette);
  document.addEventListener("keydown", (e) => {
    if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === "k") {
      e.preventDefault();
      openPalette();
    }
    // "/" focuses the page search field, like many web apps.
    if (e.key === "/" && !e.target.closest("input, textarea, select, dialog")) {
      const search = $('.main input[type="search"]');
      if (search) {
        e.preventDefault();
        search.focus();
      }
    }
  });
}

setupThemeToggle();
setupShortcuts();
enableRipples();
window.addEventListener("hashchange", navigate);
window.addEventListener("resize", () => updateNav(parseHash().path));
window.addEventListener("library:changed", refreshBadges);
navigate();
