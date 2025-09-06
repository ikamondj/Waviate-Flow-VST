"use client";

import { useState } from "react";

export default function LoginPage() {
  const [email, setEmail] = useState("");
  const [password, setPw] = useState("");
  const [loading, setLoading] = useState(false);
  const [resp, setResp] = useState<unknown>(null);
  const API = process.env.NEXT_PUBLIC_API_URL!;

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);
    try {
      const r = await fetch(API, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          func: "loginUser",
          args: { email, password },
        }),
      });
      const data = await r.json();
      setResp(data);
      // TODO: store token in cookie/localStorage if you want
    } catch (err) {
      setResp({ error: String(err) });
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-black text-white">
      <div className="max-w-md mx-auto pt-24 p-6">
        <h1 className="text-3xl font-semibold mb-6">Log in</h1>
        <form onSubmit={submit} className="grid gap-4">
          <input
            type="email"
            placeholder="Email"
            className="bg-black/30 border border-white/15 rounded-lg px-3 py-2"
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            autoComplete="email"
            required
          />
          <input
            type="password"
            placeholder="Password"
            className="bg-black/30 border border-white/15 rounded-lg px-3 py-2"
            value={password}
            onChange={(e) => setPw(e.target.value)}
            autoComplete="current-password"
            required
          />
          <button
            disabled={loading}
            className="rounded-lg px-4 py-2 border border-white/15 hover:bg-white/10 transition disabled:opacity-60"
          >
            {loading ? "Signing in…" : "Sign in"}
          </button>
        </form>
      </div>
    </div>
  );
}
