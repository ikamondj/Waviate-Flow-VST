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
-- USERS: 48-bit integer primary key (INT8 with range check)
-- ids 0 and 1 are reserved; valid range is [2, 2^64 - 1]
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS users (
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
    CHECK (id >= 16)    -- 16 .. (2^64 - 1)
);

-- Case-insensitive uniqueness helper for email (optional but recommended)
CREATE UNIQUE INDEX IF NOT EXISTS users_email_lower_uq ON users (lower(email));

-- --------------------------------------------------------------------------
-- SUBSCRIPTIONS: one active/trialing per user gates marketplace access
-- Partial unique index enforces at most one active/trialing subscription
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS subscriptions (
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
CREATE TABLE IF NOT EXISTS categories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name STRING UNIQUE NOT NULL
);

-- --------------------------------------------------------------------------
-- ENTRIES: latest asset only; natural key (creator_id, local_node_id)
-- local_node_id initially 0..999 (can ALTER to 0..16383 later)
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS entries (
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
    moderation_status STRING NOT NULL DEFAULT 'approved',  -- 'pending','rejected','approved'
    is_active BOOL NOT NULL DEFAULT TRUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_author_localnode UNIQUE (creator_id, local_node_id),
    CONSTRAINT ck_localnode_range CHECK (local_node_id BETWEEN 0 AND 999)
);

CREATE TABLE IF NOT EXISTS entry_dependencies(
    entry_id BIGINT,
    depends_on_entry_id BIGINT
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
CREATE TABLE IF NOT EXISTS tags (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name STRING UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS entry_tags (
    entry_id UUID NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    tag_id   UUID NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (entry_id, tag_id)
);

-- --------------------------------------------------------------------------
-- USER SELECTIONS: what the user wants synced/pulled locally (NOT ownership)
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS user_entry_selections (
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
CREATE TABLE IF NOT EXISTS reviews (
    review_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id BIGINT NOT NULL,
    entry_user_id BIGINT NOT NULL,
    entry_node_id BIGINT NOT NULL,
    comment TEXT,
    created_at TIMESTAMP DEFAULT now(),
    -- Add foreign keys as needed
    FOREIGN KEY (user_id) REFERENCES users(user_id),
    FOREIGN KEY (entry_user_id, entry_node_id) REFERENCES entries(user_id, node_id)
);

CREATE TABLE IF NOT EXISTS star_rating_types (
    rating_type_id SERIAL PRIMARY KEY,
    name TEXT UNIQUE NOT NULL
    -- e.g., 'artistry', 'uniqueness', 'musicality', etc.
);

CREATE TABLE IF NOT EXISTS review_star_ratings (
    review_id UUID NOT NULL,
    rating_type_id INT NOT NULL,
    stars INT NOT NULL CHECK (stars BETWEEN 1 AND 5),
    PRIMARY KEY (review_id, rating_type_id),
    FOREIGN KEY (review_id) REFERENCES reviews(review_id) ON DELETE CASCADE,
    FOREIGN KEY (rating_type_id) REFERENCES star_rating_types(rating_type_id)
);

CREATE INDEX IF NOT EXISTS reviews_by_entry ON reviews (entry_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_review_star_ratings_review_id ON review_star_ratings(review_id);

-- --------------------------------------------------------------------------
-- DOWNLOADS (optional) for popularity/analytics ("popular/new" lists)
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS downloads (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id INT8 REFERENCES users(id) ON DELETE SET NULL,
    entry_id UUID NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS downloads_by_entry_time ON downloads (entry_id, created_at DESC);

-- --------------------------------------------------------------------------
-- DAILY_RANDOM: stores random entry lists for each minute of the day
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS daily_random (
    minute INT2 PRIMARY KEY, -- 0..1439 (minute of day)
    entry_ids STRING NOT NULL -- comma-separated list of entry UUIDs for this minute
);

-- ============================================================================
-- End of schema
-- ============================================================================
