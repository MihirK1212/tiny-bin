BEGIN;

INSERT INTO tiny_bin.users (username, email)
VALUES
    ('alice',   'alice@example.com'),
    ('bob',     'bob@example.com'),
    ('charlie', 'charlie@example.com'),
    ('david',   'david@example.com'),
    ('eve',     'eve@example.com')
ON CONFLICT (username) DO NOTHING;

COMMIT;
