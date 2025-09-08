-- ============================================================================
-- Waviate Flow Marketplace - CockroachDB schema
-- Subscription unlocks all content; users select which entries to sync locally.
-- Users have 48-bit integer IDs (0 and 1 reserved). Entries use (creator_id, local_node_id).
-- Latest-asset model: entries point to the currently published asset; no version table.
-- ============================================================================

-- (Optional) create and use a dedicated database
CREATE DATABASE IF NOT EXISTS waviate_flow;
USE waviate_flow;

-- --------------------------------------------------------------------------
-- Drop in dependency order for local dev resets (safe in empty DBs)
-- --------------------------------------------------------------------------
DROP TABLE IF EXISTS downloads CASCADE;
DROP TABLE IF EXISTS reviews CASCADE;
DROP TABLE IF EXISTS user_entry_selections CASCADE;
DROP TABLE IF EXISTS entry_tags CASCADE;
DROP TABLE IF EXISTS tags CASCADE;
DROP TABLE IF EXISTS entries CASCADE;
DROP TABLE IF EXISTS categories CASCADE;
DROP TABLE IF EXISTS subscriptions CASCADE;
DROP TABLE IF EXISTS users CASCADE;

-- --------------------------------------------------------------------------
-- USERS: 48-bit integer primary key (INT8 with range check)
-- ids 0 and 1 are reserved; valid range is [2, 2^48 - 1]
-- --------------------------------------------------------------------------
CREATE TABLE users (
    id INT8 PRIMARY KEY,
    username STRING UNIQUE NOT NULL,
    email STRING UNIQUE NOT NULL,
    provider_type INT2 NOT NULL DEFAULT 0,       -- 0: basic, 1: Google, 2: Apple, etc.
    profile JSONB,
    role STRING NOT NULL DEFAULT 'user',         -- 'user','admin','moderator'
    is_banned BOOL NOT NULL DEFAULT FALSE,
    last_login_at TIMESTAMPTZ,                   -- track last login
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CHECK (id >= 16 AND id <= 281474976710655)    -- 16 .. (2^48 - 1)
);

-- Case-insensitive uniqueness helper for email (optional but recommended)
CREATE UNIQUE INDEX IF NOT EXISTS users_email_lower_uq ON users (lower(email));

-- --------------------------------------------------------------------------
-- SUBSCRIPTIONS: one active/trialing per user gates marketplace access
-- Partial unique index enforces at most one active/trialing subscription
-- --------------------------------------------------------------------------
CREATE TABLE subscriptions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id INT8 NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    status STRING NOT NULL,                      -- 'active','trialing','canceled','past_due'
    plan STRING NOT NULL DEFAULT 'pro',
    provider STRING,                             -- 'stripe','patreon', etc.
    external_id STRING,                          -- provider subscription id
    started_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    renews_at TIMESTAMPTZ,
    canceled_at TIMESTAMPTZ
);

-- one active/trialing per user
CREATE UNIQUE INDEX IF NOT EXISTS one_active_sub_per_user
    ON subscriptions (user_id)
    WHERE status IN ('active','trialing');

CREATE INDEX IF NOT EXISTS subscriptions_by_user ON subscriptions (user_id);

-- --------------------------------------------------------------------------
-- CATEGORIES (optional)
-- --------------------------------------------------------------------------
CREATE TABLE categories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name STRING UNIQUE NOT NULL
);

-- --------------------------------------------------------------------------
-- ENTRIES: latest asset only; natural key (creator_id, local_node_id)
-- local_node_id initially 0..999 (can ALTER to 0..16383 later)
-- --------------------------------------------------------------------------
CREATE TABLE entries (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    creator_id INT8 NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    local_node_id INT4 NOT NULL,
    title STRING NOT NULL,
    description STRING,
    category_id UUID REFERENCES categories(id) ON DELETE SET NULL,
    content JSONB,                               -- metadata/search facets
    asset_url STRING,                            -- current published asset
    asset_sha256 STRING,                         -- integrity check
    asset_size_bytes INT8,
    platform STRING,                             -- 'win','mac','linux','all' (optional)
    moderation_status STRING NOT NULL DEFAULT 'approved',  -- 'pending','rejected','approved'
    is_active BOOL NOT NULL DEFAULT TRUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_author_localnode UNIQUE (creator_id, local_node_id),
    CONSTRAINT ck_localnode_range CHECK (local_node_id BETWEEN 0 AND 999)
);

-- If you later want ~16k per user, run:
-- ALTER TABLE entries DROP CONSTRAINT ck_localnode_range;
-- ALTER TABLE entries ADD CONSTRAINT ck_localnode_range CHECK (local_node_id BETWEEN 0 AND 16383);

CREATE INDEX IF NOT EXISTS entries_by_creator ON entries (creator_id);
CREATE INDEX IF NOT EXISTS entries_by_creator_localnode ON entries (creator_id, local_node_id);
CREATE INDEX IF NOT EXISTS entries_active_created_at ON entries (is_active, created_at DESC);

-- JSONB inverted index (for filtering on keys in "content")
CREATE INVERTED INDEX IF NOT EXISTS entries_content_inv ON entries (content);

-- --------------------------------------------------------------------------
-- TAGS (optional) and join table
-- --------------------------------------------------------------------------
CREATE TABLE tags (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name STRING UNIQUE NOT NULL
);

CREATE TABLE entry_tags (
    entry_id UUID NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    tag_id   UUID NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (entry_id, tag_id)
);

-- --------------------------------------------------------------------------
-- USER SELECTIONS: what the user wants synced/pulled locally (NOT ownership)
-- --------------------------------------------------------------------------
CREATE TABLE user_entry_selections (
    user_id INT8 NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    entry_id UUID NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    pinned BOOL NOT NULL DEFAULT FALSE,          -- keep around
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, entry_id)
);

CREATE INDEX IF NOT EXISTS selections_by_user ON user_entry_selections (user_id);
CREATE INDEX IF NOT EXISTS selections_by_entry ON user_entry_selections (entry_id);

-- --------------------------------------------------------------------------
-- REVIEWS: one per user per entry
-- --------------------------------------------------------------------------
CREATE TABLE reviews (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    entry_id UUID NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    user_id INT8 NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    rating INT NOT NULL CHECK (rating BETWEEN 1 AND 5),
    comment STRING,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (entry_id, user_id)
);

CREATE INDEX IF NOT EXISTS reviews_by_entry ON reviews (entry_id, created_at DESC);

-- --------------------------------------------------------------------------
-- DOWNLOADS (optional) for popularity/analytics ("popular/new" lists)
-- --------------------------------------------------------------------------
CREATE TABLE downloads (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id INT8 REFERENCES users(id) ON DELETE SET NULL,
    entry_id UUID NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS downloads_by_entry_time ON downloads (entry_id, created_at DESC);

-- ============================================================================
-- End of schema
-- ============================================================================
