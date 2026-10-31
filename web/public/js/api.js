/**
 * Thin REST client for the C++ backend. Every call resolves to parsed JSON
 * or rejects with an ApiError carrying the server's (Chinese) message.
 */

export class ApiError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

async function request(method, path, body) {
  let response;
  try {
    response = await fetch(`/api${path}`, {
      method,
      headers: body ? { "Content-Type": "application/json" } : undefined,
      body: body ? JSON.stringify(body) : undefined,
    });
  } catch {
    throw new ApiError(0, "无法连接服务器，请确认后端已启动");
  }
  if (response.status === 204) return null;

  const data = await response.json().catch(() => null);
  if (!response.ok) throw new ApiError(response.status, data?.error ?? `请求失败（${response.status}）`);
  return data;
}

function query(params) {
  const entries = Object.entries(params).filter(([, v]) => v !== undefined && v !== null && v !== "" && v !== false);
  return entries.length ? `?${new URLSearchParams(entries)}` : "";
}

export const api = {
  stats: () => request("GET", "/stats"),
  activity: (days = 14) => request("GET", `/stats/activity?days=${days}&tz=${-new Date().getTimezoneOffset()}`),
  popular: (limit = 5) => request("GET", `/stats/popular?limit=${limit}`),

  books: {
    list: ({ q, category, available } = {}) => request("GET", `/books${query({ q, category, available })}`),
    create: (book) => request("POST", "/books", book),
    update: (id, book) => request("PUT", `/books/${id}`, book),
    remove: (id) => request("DELETE", `/books/${id}`),
  },

  students: {
    list: ({ q } = {}) => request("GET", `/students${query({ q })}`),
    get: (id) => request("GET", `/students/${id}`),
    create: (student) => request("POST", "/students", student),
    update: (id, student) => request("PUT", `/students/${id}`, student),
    remove: (id) => request("DELETE", `/students/${id}`),
  },

  loans: {
    list: ({ status, studentId, bookId } = {}) => request("GET", `/loans${query({ status, studentId, bookId })}`),
    borrow: ({ studentId, bookId, days }) => request("POST", "/loans", { studentId, bookId, days }),
    giveBack: (id) => request("POST", `/loans/${id}/return`),
  },
};
