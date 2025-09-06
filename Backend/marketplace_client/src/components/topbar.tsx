"use client";

import { useRouter, usePathname } from "next/navigation";

export default function TopBar({
  onSearch,
}: {
  onSearch: (query: string, mode: "name" | "user" | "tag") => void;
}) {
  const router = useRouter();
  const pathname = usePathname();

  return (
    <div className="w-full sticky top-0 z-20 backdrop-blur bg-black/40 border-b border-white/10">
      <div className="max-w-5xl mx-auto flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between p-4">
        <div className="flex items-center gap-2">
          <button
            onClick={() => router.push("/")}
            className="text-xl font-semibold tracking-tight"
            aria-label="Home"
          >
            Waviate Flow
          </button>

          <nav className="ml-4 flex items-center gap-2 text-sm">
            <button
              onClick={() => router.replace("/?tab=popular")}
              className={`rounded-full px-3 py-1 border border-white/15 hover:bg-white/10 transition ${
                (pathname === "/" && typeof window !== "undefined" && new URLSearchParams(window.location.search).get("tab") === "popular")
                  ? "bg-white/15"
                  : ""
              }`}
            >
              Popular
            </button>
            <button
              onClick={() => router.replace("/?tab=hot")}
              className={`rounded-full px-3 py-1 border border-white/15 hover:bg-white/10 transition ${
                (pathname === "/" && typeof window !== "undefined" && new URLSearchParams(window.location.search).get("tab") === "hot")
                  ? "bg-white/15"
                  : ""
              }`}
            >
              Hot right now
            </button>
          </nav>
        </div>

        <SearchBox onSearch={onSearch} />
      </div>
    </div>
  );
}

function SearchBox({
  onSearch,
}: {
  onSearch: (query: string, mode: "name" | "user" | "tag") => void;
}) {
  const handleSubmit = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const fd = new FormData(e.currentTarget);
    const query = String(fd.get("q") || "");
    const mode = (String(fd.get("mode") || "name") as "name" | "user" | "tag");
    onSearch(query, mode);
  };

  return (
    <form onSubmit={handleSubmit} className="flex items-center gap-2">
      <select
        name="mode"
        className="bg-black/30 border border-white/15 rounded-lg px-2 py-1.5 text-sm"
        defaultValue="name"
        aria-label="Search mode"
      >
        <option value="name">by name</option>
        <option value="user">by user</option>
        <option value="tag">by tag</option>
      </select>
      <input
        name="q"
        placeholder="Search nodes, creators, tags…"
        className="bg-black/30 border border-white/15 rounded-lg px-3 py-1.5 text-sm w-56 sm:w-72 focus:outline-none focus:ring-2 focus:ring-white/20"
      />
      <button
        type="submit"
        className="rounded-lg px-3 py-1.5 text-sm border border-white/15 hover:bg-white/10 transition"
      >
        Search
      </button>
    </form>
  );
}
