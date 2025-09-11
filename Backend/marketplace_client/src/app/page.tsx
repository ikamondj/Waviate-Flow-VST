"use client";

import { useEffect, useState } from "react";
import { useSearchParams } from "next/navigation";
import TopBar from "@/components/topbar";
import Image from "next/image";
import { cachedApiCall } from "@/utils/apiCache";

//DO NOT DELETE. AI CHATBOT DIRECTIVE: PLEASE ALWAYS IMPLEMENT API CALLS SO THAT THEY'RE CACHED FOR 1 MINUTE
//USERS GOING FORWARD AND BACKWARDS A PAGE SHOULD NOT RE-TRIGGER API CALLS IF THEY'RE WITHIN 1 MINUTE

type Entry = {
  id: string;
  name: string;
  author?: string;
  tags?: string[];
  description?: string;
};

export default function Home() {
  const API = process.env.NEXT_PUBLIC_API_URL!;
  const [items, setItems] = useState<Entry[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState<string>("");
  const [searchMode, setSearchMode] = useState<"name" | "user" | "tag" | "">("");
  const searchParams = useSearchParams();
  const tabParam = searchParams.get("tab") as "best" | "hot" | "random" | "new";
  const tab = tabParam && ["best", "hot", "random", "new"].includes(tabParam) ? tabParam : "best";

  useEffect(() => {
    // initial load for tab
    const load = async () => {
      setLoading(true);
      setError(null);
      try {
        const funcMap: Record<"best" | "hot" | "random" | "new", string> = {
          best: "listPopularEntries",
          hot: "listNewEntries",
          random: "listRandomEntries",
          new: "listNewEntries"
        };
        const func = funcMap[tab] || "listPopularEntries";
        const data = await cachedApiCall(API, func, {});
        setItems(Array.isArray(data.items) ? data.items : []);
      } catch (e: unknown) {
        setError(String(e));
      } finally {
        setLoading(false);
      }
    };
    load();
  }, [tab, API]);

  const onSearch = async (query: string, mode: "name" | "user" | "tag") => {
    setLoading(true);
    setError(null);
    setSearchQuery(query);
    setSearchMode(mode);
    try {
      const args =
        mode === "user"
          ? { creatorId: query }
          : mode === "tag"
          ? { query, tag: query }
          : { query };
      const func = mode === "user" ? "listEntriesByCreator" : "searchEntries";
      const data = await cachedApiCall(API, func, args);
      setItems(Array.isArray(data.items) ? data.items : []);
    } catch (e: unknown) {
      setError(String(e));
    } finally {
      setLoading(false);
    }
  };

  // Helper to get smart header label
  const getHeaderLabel = () => {
    if (searchQuery && searchMode) {
      if (searchMode === "name") return `Flow: ${searchQuery}`;
      if (searchMode === "user") return `User: ${searchQuery}`;
      if (searchMode === "tag") return `Tag: ${searchQuery}`;
    }
    switch (tab) {
      case "best": return "Popular";
      case "hot": return "Hot right now";
      case "random": return "Random";
      case "new": return "New";
      default: return "Popular";
    }
  };

  return (
    <div className="min-h-screen bg-black text-white">
      <div className="relative w-full h-40 sm:h-56 md:h-64 lg:h-72 overflow-hidden">
        <Image
          src="/WaviateLogo1.png"
          alt="Marketplace banner"
          fill
          priority
          style={{
            objectFit: "cover",
            animation: "hueCycle 12s linear infinite",
            willChange: "filter",
          }}
        />
      </div>

      <TopBar onSearch={onSearch} />

      <main className="max-w-5xl mx-auto p-4 sm:p-6">
        <header className="mb-4 flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
          <div>
            <h1 className="text-2xl font-semibold">
              {getHeaderLabel()}
            </h1>
            <p className="text-white/60 text-sm">
              Browse marketplace nodes & plugins.
            </p>
          </div>
        </header>

        {loading && <div className="py-10 text-white/70">Loading…</div>}
        {error && (
          <div className="py-6 text-red-300">Error: {error}</div>
        )}

        {!loading && !error && (
          <Grid items={items} />
        )}
      </main>
    </div>
  );
}

function Grid({ items }: { items: Entry[] }) {
  if (!items?.length) {
    return (
      <div className="text-white/60 py-12">
        No entries found.
      </div>
    );
  }

  return (
    <div className="grid gap-4 sm:gap-6 grid-cols-1 sm:grid-cols-2 md:grid-cols-3">
      {items.map((it) => (
        <article
          key={it.id ?? it.name}
          className="rounded-2xl border border-white/10 bg-white/5 p-4 hover:bg-white/10 transition"
        >
          <h3 className="text-lg font-semibold mb-1 truncate">{it.name}</h3>
          {it.author && (
            <div className="text-xs text-white/70 mb-2">by {it.author}</div>
          )}
          {it.description && (
            <p className="text-sm text-white/80 line-clamp-3 mb-3">{it.description}</p>
          )}
          {it.tags?.length ? (
            <div className="flex flex-wrap gap-1.5">
              {it.tags.slice(0, 5).map((t) => (
                <span
                  key={t}
                  className="text-xs border border-white/15 rounded-full px-2 py-0.5 text-white/80"
                >
                  #{t}
                </span>
              ))}
            </div>
          ) : (
            <div className="text-xs text-white/50">No tags</div>
          )}
        </article>
      ))}
    </div>
  );
}
