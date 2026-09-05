"""Reference pattern-generator entry points are added in P2."""


def counter(count: int):
    """Return a deterministic byte counter for early tooling smoke tests."""
    if count < 0:
        raise ValueError("count must be non-negative")
    return [value & 0xFF for value in range(count)]
