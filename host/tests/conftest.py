"""Shared fixtures: mock analyzer server over an ephemeral TCP port."""

from __future__ import annotations

import pytest

from .mock_analyzer import MockAnalyzer


@pytest.fixture
def analyzer():
    """Factory: analyzer(scenario) -> running MockAnalyzer, auto-closed."""
    servers: list[MockAnalyzer] = []

    def start(scenario: str) -> MockAnalyzer:
        mock = MockAnalyzer(scenario)
        mock.__enter__()
        servers.append(mock)
        return mock

    yield start
    for mock in servers:
        mock.__exit__(None, None, None)
