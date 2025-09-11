// Centralized API cache utility for 1-minute caching

const CACHE_DURATION = 60 * 1000; // 1 minute in ms

// Cache key: stringified function name + args
const apiCache: Record<string, { timestamp: number; data: any }> = {};

function getCacheKey(func: string, args: any) {
  return `${func}:${JSON.stringify(args)}`;
}

export async function cachedApiCall(
  apiUrl: string,
  func: string,
  args: never
): Promise<any> {
  const key = getCacheKey(func, args);
  const now = Date.now();
  const cached = apiCache[key];
  if (cached && now - cached.timestamp < CACHE_DURATION) {
    return cached.data;
  }
  // Make API call
  const r = await fetch(apiUrl, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ func, args }),
  });
  const data = await r.json();
  // Only cache successful responses
  if (r.ok) {
    apiCache[key] = { timestamp: now, data };
  }
  return data;
}

