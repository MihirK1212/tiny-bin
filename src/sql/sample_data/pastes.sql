BEGIN;

INSERT INTO tiny_bin.pastes (id, content)
VALUES
    ('a1000001-0000-4000-8000-000000000001', 'sample TinyBin paste')
ON CONFLICT (id) DO NOTHING;

COMMIT;
