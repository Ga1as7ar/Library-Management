/**
 * Motion helpers. Everything here degrades to an instant state change when the
 * user prefers reduced motion or the browser lacks the API.
 */

export const reducedMotion = () => matchMedia("(prefers-reduced-motion: reduce)").matches;

/** Runs `update` inside a View Transition when supported. */
export function transition(update) {
  if (!document.startViewTransition || reducedMotion()) {
    update();
    return Promise.resolve();
  }
  return document.startViewTransition(update).finished.catch(() => {});
}

/** Gives `.enter` children a stagger index (--i) so they cascade in. */
export function stagger(root, selector = ".enter") {
  root.querySelectorAll(selector).forEach((el, i) => el.style.setProperty("--i", i));
}

/** Animates every [data-count] element from 0 to its value with an ease-out curve. */
export function countUp(root, duration = 1100) {
  const targets = [...root.querySelectorAll("[data-count]")];
  if (reducedMotion()) {
    targets.forEach((el) => (el.textContent = el.dataset.count));
    return;
  }
  const start = performance.now();
  const ease = (t) => 1 - Math.pow(1 - t, 4);
  const frame = (now) => {
    const t = Math.min(1, (now - start) / duration);
    targets.forEach((el) => (el.textContent = Math.round(Number(el.dataset.count) * ease(t))));
    if (t < 1) requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}

/** Material-style ripple from the pointer position on any .btn. */
export function enableRipples() {
  document.addEventListener("pointerdown", (event) => {
    const button = event.target.closest(".btn");
    if (!button || button.disabled || reducedMotion()) return;
    const rect = button.getBoundingClientRect();
    const size = Math.max(rect.width, rect.height) * 2.2;
    const ripple = document.createElement("span");
    ripple.className = "ripple";
    ripple.style.cssText = `width:${size}px;height:${size}px;left:${event.clientX - rect.left - size / 2}px;top:${event.clientY - rect.top - size / 2}px`;
    button.append(ripple);
    ripple.addEventListener("animationend", () => ripple.remove(), { once: true });
  });
}

/** Positions a sliding indicator under the active element of a group. */
export function slideIndicator(indicator, active, axis = "y") {
  if (!indicator) return;
  if (!active) {
    indicator.style.opacity = "0";
    return;
  }
  indicator.style.opacity = "1";
  if (axis === "y") {
    indicator.style.transform = `translateY(${active.offsetTop}px)`;
  } else {
    indicator.style.width = `${active.offsetWidth}px`;
    indicator.style.transform = `translateX(${active.offsetLeft}px)`;
  }
}

/**
 * Turns a .segmented group into an animated control: inserts a thumb that
 * slides to the pressed button. Returns a function to call after changing aria-pressed.
 */
export function animateSegmented(group) {
  const thumb = document.createElement("span");
  thumb.className = "segmented__thumb";
  group.prepend(thumb);
  const sync = () => slideIndicator(thumb, group.querySelector('[aria-pressed="true"]'), "x");
  // Initial placement without animation.
  thumb.style.transition = "none";
  requestAnimationFrame(() => {
    sync();
    requestAnimationFrame(() => (thumb.style.transition = ""));
  });
  return sync;
}

/** Smoothly removes an element (collapse + fade), resolving when done. */
export function leave(element) {
  return new Promise((resolve) => {
    if (!element || reducedMotion()) return resolve();
    element.classList.add("is-leaving");
    element.addEventListener("animationend", resolve, { once: true });
    setTimeout(resolve, 500);  // safety net
  });
}
