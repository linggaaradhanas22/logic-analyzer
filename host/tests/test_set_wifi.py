"""Host tests for the tools/set_wifi.py sdkconfig patcher."""

from __future__ import annotations

from tools.set_wifi import apply, kconfig_string, replace


def test_replace_updates_existing_values() -> None:
    text = 'CONFIG_LA_WIFI_SSID="old"\nCONFIG_LA_WIFI_PASSWORD="old"\n'
    text = replace(text, "SSID", "new net")
    text = replace(text, "PASSWORD", 'p"w\\d')
    assert 'CONFIG_LA_WIFI_SSID="new net"' in text
    assert 'CONFIG_LA_WIFI_PASSWORD="p\\"w\\\\d"' in text
    assert text.count("CONFIG_LA_WIFI_SSID=") == 1


def test_replace_appends_missing_lines() -> None:
    text = "# generated\n"
    text = replace(text, "SSID", "net")
    text = replace(text, "PASSWORD", "")
    assert text.endswith(
        'CONFIG_LA_WIFI_SSID="net"\nCONFIG_LA_WIFI_PASSWORD=""\n'
    )


def test_kconfig_string_escapes() -> None:
    assert kconfig_string("a\\b\"c") == '"a\\\\b\\"c"'


def test_apply_patches_sdkconfig(tmp_path) -> None:
    sdkconfig = tmp_path / "sdkconfig"
    sdkconfig.write_text('CONFIG_LA_WIFI_SSID="old"\n', encoding="utf-8")
    apply(sdkconfig, "new", "secret")
    text = sdkconfig.read_text(encoding="utf-8")
    assert 'CONFIG_LA_WIFI_SSID="new"' in text
    assert 'CONFIG_LA_WIFI_PASSWORD="secret"' in text
