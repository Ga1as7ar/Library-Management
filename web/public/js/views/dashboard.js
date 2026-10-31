import { api } from "../api.js";
import { cover, daysUntil, formatDate, formatShortDate, greeting, html, icon } from "../dom.js";
import { countUp, leave, stagger } from "../motion.js";
import { emptyState, errorState } from "../ui.js";
import { borrowFlow, returnFlow } from "./actions.js";

const DAYS = 14;
const RING_RADIUS = 66;
const RING_LENGTH = 2 * Math.PI * RING_RADIUS;

/** Smooth area path (0..100 × 0..36 viewBox) for a stat-card sparkline. */
function sparkPath(values) {
  const max = Math.max(1, ...values);
  const step = 100 / (values.length - 1);
  const points = values.map((v, i) => [i * step, 34 - (v / max) * 26]);
  let d = `M0,36 L${points[0][0]},${points[0][1]}`;
  for (let i = 1; i < points.length; i++) {
    const [x0, y0] = points[i - 1];
    const [x1, y1] = points[i];
    const cx = (x0 + x1) / 2;
    d += ` C${cx},${y0} ${cx},${y1} ${x1},${y1}`;
  }
  return `${d} L100,36 Z`;
}

const statCard = ({ label, value, meta, glyph, variant = "", spark }) => html`
  <div class="stat enter ${variant}">
    <span class="stat__icon">${icon(glyph)}</span>
    <div class="stat__label">${label}</div>
    <div class="stat__value num" data-count="${value}">0</div>
    <div class="stat__meta">${meta}</div>
    ${spark ? html`<svg class="stat__spark" viewBox="0 0 100 36" preserveAspectRatio="none" aria-hidden="true"><path d="${sparkPath(spark)}"/></svg>` : ""}
  </div>`;

function dueBadge(loan) {
  const days = daysUntil(loan.dueAt);
  if (days < 0) return html`<span class="badge badge--danger">逾期 ${-days} 天</span>`;
  if (days <= 3) return html`<span class="badge badge--warning">${days === 0 ? "今天到期" : `${days} 天后到期`}</span>`;
  return html`<span class="badge">${formatDate(loan.dueAt)}</span>`;
}

function activityChart(activity) {
  const max = Math.max(1, ...activity);
  const today = new Date();
  const dayOf = (i) => new Date(today.getFullYear(), today.getMonth(), today.getDate() - (activity.length - 1 - i));
  const total = activity.reduce((a, b) => a + b, 0);
  return html`
    <div class="panel enter">
      <div class="panel__head">
        <div><h2>借阅趋势</h2><p>近 ${activity.length} 天共借出 ${total} 次</p></div>
        <span class="badge badge--accent">${icon("chart-bar")}每日</span>
      </div>
      <div class="chart">
        <div class="chart__bars">
          ${activity.map((v, i) => html`<div class="chart__bar ${v ? "" : "is-zero"} ${i === activity.length - 1 ? "is-today" : ""}"
              style="--v:${v / max};--i:${i}" data-tip="${formatShortDate(dayOf(i))} · ${v} 次"></div>`)}
        </div>
        <div class="chart__axis">${activity.map((_, i) => html`<span>${i % 2 === activity.length % 2 ? "" : dayOf(i).getDate()}</span>`)}</div>
      </div>
    </div>`;
}

function render(stats, loans, activity, popular) {
  const lent = stats.totalCopies - stats.availableCopies;
  const ratio = stats.totalCopies ? lent / stats.totalCopies : 0;
  const upcoming = loans.slice(0, 5);
  const today = activity.at(-1) ?? 0;

  return html`
    <section class="hero enter">
      <div class="hero__aurora"></div><div class="hero__grid"></div>
      <div class="hero__books" aria-hidden="true">
        ${[46, 62, 54, 70, 40, 58, 66].map((h, i) => html`<span style="height:${h}px;--i:${i}"></span>`)}
      </div>
      <div>
        <div class="hero__eyebrow">${new Intl.DateTimeFormat("zh-CN", { month: "long", day: "numeric", weekday: "long" }).format(new Date())}</div>
        <h1>${greeting()}，今天已借出 ${today} 本</h1>
        <p>${stats.overdueLoans ? `有 ${stats.overdueLoans} 本图书逾期未还，记得提醒读者。` : "所有在借图书都在期限内，一切井然有序。"}</p>
      </div>
      <button class="btn" data-action="borrow">${icon("book-upload")}办理借阅</button>
    </section>

    <section class="stats">
      ${statCard({ label: "在借图书", value: stats.activeLoans, meta: `${stats.students} 位读者`, glyph: "book-upload", spark: activity })}
      ${statCard({ label: "馆藏品种", value: stats.titles, meta: `共 ${stats.totalCopies} 册`, glyph: "books" })}
      ${statCard({ label: "可借册数", value: stats.availableCopies, meta: `已借出 ${lent} 册`, glyph: "book-2" })}
      ${statCard({ label: "逾期未还", value: stats.overdueLoans, meta: stats.overdueLoans ? "需尽快催还" : "一切正常", glyph: "clock", variant: stats.overdueLoans ? "stat--alert" : "" })}
    </section>

    <section class="dash-grid">
      <div class="dash-col">
        ${activityChart(activity)}
        <div class="panel enter">
          <div class="panel__head"><div><h2>即将到期</h2><p>按应还日期排序 · 点击归还</p></div><a class="btn btn--ghost btn--sm" href="#/loans">全部借阅${icon("arrow-right")}</a></div>
          ${upcoming.length
            ? html`<ul class="due-list">${upcoming.map((loan) => html`
                <li data-loan="${loan.id}">
                  ${cover(loan.bookTitle)}
                  <div class="grow">
                    <div class="cell-title">${loan.bookTitle}</div>
                    <div class="cell-sub">${loan.studentName} · 借于 ${formatDate(loan.borrowedAt)}</div>
                  </div>
                  ${dueBadge(loan)}
                  <button class="btn btn--sm" data-return="${loan.id}">${icon("book-download")}归还</button>
                </li>`)}</ul>`
            : emptyState({ glyph: "circle-check", title: "没有在借图书", text: "所有图书都已归还" })}
        </div>
      </div>

      <div class="dash-col">
        <div class="panel enter">
          <div class="panel__head"><div><h2>流通率</h2><p>借出册数 / 馆藏总册数</p></div></div>
          <div class="ring">
            <div class="ring__wrap">
              <svg viewBox="0 0 160 160" aria-hidden="true">
                <circle class="track" cx="80" cy="80" r="${RING_RADIUS}"/>
                <circle class="value" cx="80" cy="80" r="${RING_RADIUS}"
                        stroke-dasharray="${RING_LENGTH}" stroke-dashoffset="${RING_LENGTH}"
                        data-offset="${RING_LENGTH * (1 - ratio)}"/>
              </svg>
              <div class="ring__center"><div class="ring__pct num"><span data-count="${Math.round(ratio * 100)}">0</span>%</div><div class="muted">已借出</div></div>
            </div>
            <div class="ring__legend"><span>借出 ${lent}</span><span>在馆 ${stats.availableCopies}</span></div>
          </div>
        </div>

        <div class="panel enter">
          <div class="panel__head"><div><h2>热门图书</h2><p>累计借阅次数</p></div>${icon("trophy", "muted")}</div>
          ${popular.length
            ? popular.map((book, i) => html`
                <a class="rank" href="#/books?q=${encodeURIComponent(book.title)}">
                  <span class="rank__no">${i + 1}</span>
                  ${cover(book.title)}
                  <div class="grow" style="min-width:0;flex:1"><div class="cell-title" style="white-space:nowrap;overflow:hidden;text-overflow:ellipsis">${book.title}</div><div class="cell-sub">${book.author || "佚名"}</div></div>
                  <span class="badge ${book.availableCopies ? "badge--accent" : ""}">可借 ${book.availableCopies}</span>
                </a>`)
            : emptyState({ glyph: "books", title: "暂无数据", text: "借出图书后这里会出现排行" })}
        </div>
      </div>
    </section>`;
}

export function dashboardView(root) {
  root.innerHTML = html`<div class="hero"><div class="hero__aurora"></div><div><span class="skeleton" style="width:160px;opacity:.3"></span></div></div>`;

  const refresh = async () => {
    try {
      const [stats, loans, activity, popular] = await Promise.all([
        api.stats(), api.loans.list({ status: "active" }), api.activity(DAYS), api.popular(5),
      ]);
      root.loans = loans;
      root.innerHTML = render(stats, loans, activity, popular);
      stagger(root);
      countUp(root);
      requestAnimationFrame(() => requestAnimationFrame(() => {
        const ring = root.querySelector(".ring .value");
        if (ring) ring.style.strokeDashoffset = ring.dataset.offset;
      }));
    } catch (error) {
      root.innerHTML = errorState(error);
    }
  };

  root.addEventListener("click", async (event) => {
    const target = event.target.closest("[data-action], [data-return]");
    if (!target) return;
    if (target.dataset.return) {
      const loan = root.loans.find((l) => l.id === Number(target.dataset.return));
      if (await returnFlow(loan)) {
        await leave(target.closest("li"));
        refresh();
      }
    } else if (await borrowFlow()) {
      refresh();
    }
  });

  return refresh();
}
