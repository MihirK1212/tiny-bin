BEGIN;

-- Create application schema
CREATE SCHEMA tiny_bin;

-- Users table
CREATE TABLE tiny_bin.users (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    username    TEXT NOT NULL UNIQUE,
    email       TEXT NOT NULL UNIQUE,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Posts table
CREATE TABLE tiny_bin.posts (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id     BIGINT NOT NULL REFERENCES tiny_bin.users(id),
    title       TEXT NOT NULL,
    content     TEXT,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Indexes
CREATE INDEX idx_posts_user_id ON tiny_bin.posts(user_id);

COMMIT;
