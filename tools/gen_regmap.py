"""Generate the shared register-map views from docs/regmap.yaml.

This is the only supported writer for generated register files. The generator
is deliberately deterministic so running it twice produces no diff.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT_ROOT / "docs" / "regmap.yaml"
GENERATED = (
    PROJECT_ROOT / "common" / "la_proto" / "include" / "la_regs.h",
    PROJECT_ROOT / "fpga" / "rtl" / "la_regs_pkg.sv",
    PROJECT_ROOT / "tools" / "models" / "regs.py",
    PROJECT_ROOT / "docs" / "regmap.md",
)
NAME_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
CONSTRAINT_KEYS = {"min", "max", "allowed_mask", "nonzero_when"}


def _load_source() -> dict[str, Any]:
    try:
        import yaml
    except ImportError as exc:  # pragma: no cover - depends on environment
        raise RuntimeError(
            "PyYAML is required for register generation; install the project "
            "environment with `python -m pip install -e .`"
        ) from exc

    with SOURCE.open("r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle)
    if not isinstance(data, dict):
        raise ValueError("register map must be a mapping")
    return data


def _integer(value: Any, label: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{label} must be an integer")
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as exc:
            raise ValueError(f"{label} is not an integer: {value!r}") from exc
    raise ValueError(f"{label} must be an integer")


def _constraints(raw: Any, label: str) -> dict[str, Any]:
    if raw is None:
        return {}
    if not isinstance(raw, dict):
        raise ValueError(f"{label}.constraints must be a mapping")
    unknown = set(raw) - CONSTRAINT_KEYS
    if unknown:
        raise ValueError(f"{label}.constraints has unknown keys: {sorted(unknown)}")
    result: dict[str, Any] = {}
    for key in ("min", "max", "allowed_mask"):
        if key in raw:
            value = _integer(raw[key], f"{label}.constraints.{key}")
            if value < 0 or value > 0xFFFFFFFF:
                raise ValueError(f"{label}.constraints.{key} must fit in 32 bits")
            result[key] = value
    if "min" in result and "max" in result and result["min"] > result["max"]:
        raise ValueError(f"{label}.constraints.min exceeds max")
    if "nonzero_when" in raw:
        value = raw["nonzero_when"]
        if not isinstance(value, str) or not value:
            raise ValueError(f"{label}.constraints.nonzero_when must be a field name")
        result["nonzero_when"] = value
    return result


def _validate(data: dict[str, Any]) -> tuple[int, int, list[dict[str, Any]]]:
    base = _integer(data.get("base_address"), "base_address")
    id_value = _integer(data.get("id_value"), "id_value")
    raw_registers = data.get("registers")
    if not isinstance(raw_registers, list) or not raw_registers:
        raise ValueError("registers must be a non-empty list")

    registers: list[dict[str, Any]] = []
    offsets: set[int] = set()
    names: set[str] = set()
    for index, raw in enumerate(raw_registers):
        if not isinstance(raw, dict):
            raise ValueError(f"register {index} must be a mapping")
        name = raw.get("name")
        if not isinstance(name, str) or not NAME_RE.fullmatch(name):
            raise ValueError(f"register {index} has invalid name: {name!r}")
        offset = _integer(raw.get("offset"), f"{name}.offset")
        reset = _integer(raw.get("reset", 0), f"{name}.reset")
        access = raw.get("access")
        if access not in {"RO", "RW", "W1C"}:
            raise ValueError(f"{name}.access must be RO, RW or W1C")
        if offset < 0 or offset % 4:
            raise ValueError(f"{name}.offset must be a non-negative 32-bit word offset")
        if reset < 0 or reset > 0xFFFFFFFF:
            raise ValueError(f"{name}.reset must fit in 32 bits")
        if offset in offsets:
            raise ValueError(f"duplicate register offset: 0x{offset:X}")
        if name in names:
            raise ValueError(f"duplicate register name: {name}")
        offsets.add(offset)
        names.add(name)

        fields = raw.get("fields", [])
        if not isinstance(fields, list):
            raise ValueError(f"{name}.fields must be a list")
        clean_fields: list[dict[str, Any]] = []
        field_names: set[str] = set()
        occupied = 0
        for field in fields:
            if not isinstance(field, dict):
                raise ValueError(f"{name} field must be a mapping")
            field_name = field.get("name")
            if not isinstance(field_name, str) or not NAME_RE.fullmatch(field_name):
                raise ValueError(f"{name} has invalid field name: {field_name!r}")
            lsb = _integer(field.get("lsb"), f"{name}.{field_name}.lsb")
            width = _integer(field.get("width"), f"{name}.{field_name}.width")
            if field_name in field_names:
                raise ValueError(f"duplicate field name: {name}.{field_name}")
            if lsb < 0 or width <= 0 or lsb + width > 32:
                raise ValueError(f"{name}.{field_name} is outside a 32-bit register")
            mask = ((1 << width) - 1) << lsb
            if occupied & mask:
                raise ValueError(f"overlapping field: {name}.{field_name}")
            field_names.add(field_name)
            occupied |= mask
            raw_values = field.get("values", {})
            if not isinstance(raw_values, dict):
                raise ValueError(f"{name}.{field_name}.values must be a mapping")
            values: dict[int, str] = {}
            for raw_value, value_name in raw_values.items():
                enum_value = _integer(raw_value, f"{name}.{field_name}.values key")
                if enum_value < 0 or enum_value >= (1 << width):
                    raise ValueError(f"{name}.{field_name} enum value is outside the field")
                if not isinstance(value_name, str) or not NAME_RE.fullmatch(value_name):
                    raise ValueError(f"{name}.{field_name} has invalid enum name: {value_name!r}")
                if enum_value in values or value_name in values.values():
                    raise ValueError(f"duplicate enum value or name: {name}.{field_name}")
                values[enum_value] = value_name
            clean_fields.append(
                {
                    "name": field_name,
                    "lsb": lsb,
                    "width": width,
                    "mask": mask,
                    "values": values,
                }
            )
        registers.append(
            {
                "name": name,
                "offset": offset,
                "reset": reset,
                "access": access,
                "description": str(raw.get("description", "")),
                "fields": clean_fields,
                "constraints": _constraints(raw.get("constraints"), name),
            }
        )

    registers.sort(key=lambda item: item["offset"])
    return base, id_value, registers


def _hex(value: int, digits: int = 8) -> str:
    return f"0x{value:0{digits}X}"


def _c_header(base: int, id_value: int, registers: list[dict[str, Any]]) -> str:
    lines = [
        "/* Generated by tools/gen_regmap.py. Do not edit. */",
        "#ifndef LA_REGS_H",
        "#define LA_REGS_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define LA_REG_BASE_ADDRESS UINT32_C({_hex(base)})",
        f"#define LA_REG_ID_VALUE UINT32_C({_hex(id_value)})",
        "",
    ]
    for register in registers:
        name = register["name"]
        lines.extend(
            [
                f"#define LA_REG_{name}_OFFSET UINT32_C({_hex(register['offset'])})",
                f"#define LA_REG_{name}_RESET UINT32_C({_hex(register['reset'])})",
                f"#define LA_REG_{name}_ACCESS_{register['access']} 1",
            ]
        )
        for field in register["fields"]:
            lines.extend(
                [
                    f"#define LA_REG_{name}_{field['name']}_LSB {field['lsb']}u",
                    f"#define LA_REG_{name}_{field['name']}_WIDTH {field['width']}u",
                    f"#define LA_REG_{name}_{field['name']}_MASK UINT32_C({_hex(field['mask'])})",
                ]
            )
            for value, value_name in sorted(field["values"].items()):
                lines.append(
                    f"#define LA_REG_{name}_{field['name']}_VALUE_{value_name} {value}u"
                )
        for key in ("min", "max", "allowed_mask"):
            if key in register["constraints"]:
                lines.append(
                    f"#define LA_REG_{name}_{key.upper()} UINT32_C({_hex(register['constraints'][key])})"
                )
        lines.append("")
    lines.append("#endif /* LA_REGS_H */")
    return "\n".join(lines) + "\n"


def _sv_package(base: int, id_value: int, registers: list[dict[str, Any]]) -> str:
    lines = [
        "// Generated by tools/gen_regmap.py. Do not edit.",
        "package la_regs_pkg;",
        f"  localparam logic [31:0] LA_REG_BASE_ADDRESS = 32'h{base:08X};",
        f"  localparam logic [31:0] LA_REG_ID_VALUE = 32'h{id_value:08X};",
        "",
    ]
    for register in registers:
        name = register["name"]
        lines.extend(
            [
                f"  localparam logic [31:0] LA_REG_{name}_OFFSET = 32'h{register['offset']:08X};",
                f"  localparam logic [31:0] LA_REG_{name}_RESET = 32'h{register['reset']:08X};",
            ]
        )
        for field in register["fields"]:
            lines.append(
                f"  localparam logic [31:0] LA_REG_{name}_{field['name']}_MASK = 32'h{field['mask']:08X};"
            )
            for value, value_name in sorted(field["values"].items()):
                lines.append(
                    f"  localparam logic [31:0] LA_REG_{name}_{field['name']}_VALUE_{value_name} = 32'd{value};"
                )
        for key in ("min", "max", "allowed_mask"):
            if key in register["constraints"]:
                lines.append(
                    f"  localparam logic [31:0] LA_REG_{name}_{key.upper()} = 32'h{register['constraints'][key]:08X};"
                )
        lines.append("")
    lines.append("endpackage : la_regs_pkg")
    return "\n".join(lines) + "\n"


def _python_module(base: int, id_value: int, registers: list[dict[str, Any]]) -> str:
    lines = [
        '"""Generated by tools/gen_regmap.py. Do not edit."""',
        "BASE_ADDRESS = 0x%08X" % base,
        "ID_VALUE = 0x%08X" % id_value,
        "",
        "REGISTERS = {",
    ]
    for register in registers:
        lines.append(
            "    %r: {\"offset\": 0x%X, \"reset\": 0x%08X, \"access\": %r, \"constraints\": %r},"
            % (
                register["name"],
                register["offset"],
                register["reset"],
                register["access"],
                register["constraints"],
            )
        )
    lines.extend(["}", "", "FIELD_MASKS = {"])
    for register in registers:
        for field in register["fields"]:
            lines.append(
                "    %r: 0x%08X," % (f"{register['name']}_{field['name']}", field["mask"])
            )
    lines.extend(["}", "", "FIELD_VALUES = {"])
    for register in registers:
        for field in register["fields"]:
            if field["values"]:
                lines.append(
                    "    %r: %r," % (
                        f"{register['name']}_{field['name']}",
                        {name: value for value, name in field["values"].items()},
                    )
                )
    lines.extend(["}", ""])
    return "\n".join(lines)


def _markdown(base: int, id_value: int, registers: list[dict[str, Any]]) -> str:
    lines = [
        "# Register Map",
        "",
        "Generated from `docs/regmap.yaml`; do not edit this file directly.",
        "",
        f"Base address: `{_hex(base)}`  ",
        f"ID value: `{_hex(id_value)}`",
        "",
        "| Offset | Name | Access | Reset | Description |",
        "|---:|---|:---:|---:|---|",
    ]
    for register in registers:
        description = register["description"].replace("|", "\\|")
        lines.append(
            f"| `{_hex(register['offset'])}` | `{register['name']}` | {register['access']} "
            f"| `{_hex(register['reset'])}` | {description} |"
        )
        for field in register["fields"]:
            lines.append(
                f"| | `{register['name']}.{field['name']}` | field | "
                f"bits {field['lsb']}:{field['lsb'] + field['width'] - 1} | "
                f"mask `{_hex(field['mask'])}` |"
            )
            for value, value_name in sorted(field["values"].items()):
                lines.append(
                    f"| | `{register['name']}.{field['name']}={value}` | enum | {value_name} | |"
                )
        constraints = register["constraints"]
        if constraints:
            rendered = []
            for key in ("min", "max", "allowed_mask", "nonzero_when"):
                if key in constraints:
                    value = constraints[key]
                    rendered.append(
                        f"{key} `{_hex(value) if isinstance(value, int) else value}`"
                    )
            lines.append(f"| | `{register['name']}` | constraint | | {'; '.join(rendered)} |")
    return "\n".join(lines) + "\n"


def _write_if_changed(path: Path, content: str) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    old = path.read_text(encoding="utf-8") if path.exists() else None
    if old == content:
        return False
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)
    return True


def _render() -> tuple[tuple[Path, str], ...]:
    base, id_value, registers = _validate(_load_source())
    return tuple(
        zip(
            GENERATED,
            (
                _c_header(base, id_value, registers),
                _sv_package(base, id_value, registers),
                _python_module(base, id_value, registers),
                _markdown(base, id_value, registers),
            ),
        )
    )


def generate() -> list[Path]:
    rendered = _render()
    changed: list[Path] = []
    for path, content in rendered:
        if _write_if_changed(path, content):
            changed.append(path)
    return changed


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if generated files are stale")
    args = parser.parse_args(argv)
    try:
        if args.check:
            stale = []
            for path, content in _render():
                if not path.exists() or path.read_text(encoding="utf-8") != content:
                    stale.append(path)
            if stale:
                print("Generated files are stale:")
                for path in stale:
                    print(f"  {path.relative_to(PROJECT_ROOT)}")
                return 1
            print("Register map is up to date.")
            return 0
        changed = generate()
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"gen_regmap: error: {exc}", file=sys.stderr)
        return 1
    if changed:
        print("Generated:")
        for path in changed:
            print(f"  {path.relative_to(PROJECT_ROOT)}")
    else:
        print("Register map already generated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
