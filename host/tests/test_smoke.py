def test_host_package_imports() -> None:
    import host.la_cli

    assert callable(host.la_cli.main)
