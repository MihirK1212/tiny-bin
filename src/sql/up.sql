BEGIN;

CREATE SCHEMA tiny_bin;

CREATE TABLE tiny_bin.pastes (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    content TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT valid_content_length CHECK (octet_length(content) BETWEEN 1 AND 10240)
);

COMMIT;
