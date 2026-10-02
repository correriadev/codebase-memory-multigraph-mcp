"""Disable archived/recreated Codex skills without changing unrelated settings."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tomllib

parser = argparse.ArgumentParser()
parser.add_argument("backup", type=Path, nargs="+")
args = parser.parse_args()
profile = Path.home().resolve()
backups = [path.resolve() for path in args.backup]
for path in backups:
    if not path.is_relative_to(profile / ".skill-backups"):
        raise SystemExit("Backup outside the expected directory")
backup = backups[0]
entries = []
for path in backups:
    entries.extend(json.loads((path / "manifest.json").read_text(encoding="utf-8-sig"))["Entries"])
config_path = profile / ".codex" / "config.toml"
original = config_path.read_bytes()
text = original.decode("utf-8-sig")
parsed = tomllib.loads(text)
disabled_paths = set()
for entry in entries:
    source = Path(entry["Source"])
    if source.is_relative_to(profile / ".codex") or source.is_relative_to(profile / ".agents"):
        for file in entry["Files"]:
            relative = Path(file["Relative"])
            if relative.name == "SKILL.md":
                disabled_paths.add((source / relative).as_posix())
system_root = profile / ".codex" / "skills" / ".system"
disabled_paths.update(p.as_posix() for p in system_root.rglob("SKILL.md"))
existing = {Path(item["path"]).as_posix(): item for item in parsed.get("skills", {}).get("config", [])}
for path in disabled_paths:
    if path in existing and existing[path].get("enabled", True):
        raise SystemExit(f"Existing enabled override requires manual reconciliation: {path}")
new_paths = sorted(disabled_paths - existing.keys())
block = "\n# >>> fractal-skill-isolation >>>\n"
for path in new_paths:
    block += "[[skills.config]]\npath = " + json.dumps(path, ensure_ascii=False) + "\nenabled = false\n\n"
block += "# <<< fractal-skill-isolation <<<\n"
updated = text + block if new_paths else text
validated = tomllib.loads(updated)
overrides = {Path(i["path"]).as_posix(): i for i in validated["skills"]["config"]}
assert all(overrides[path]["enabled"] is False for path in disabled_paths)
config_backup = backup / "codex-config-before-isolation.toml"
if config_backup.exists():
    raise SystemExit("Config backup already exists; refusing overwrite")
shutil.copy2(config_path, config_backup)
assert config_backup.read_bytes() == original
config_path.write_bytes(updated.encode("utf-8"))
tomllib.loads(config_path.read_text(encoding="utf-8"))
report = {
    "config": str(config_path),
    "backup": str(config_backup),
    "before_sha256": hashlib.sha256(original).hexdigest(),
    "after_sha256": hashlib.sha256(config_path.read_bytes()).hexdigest(),
    "disabled_paths": sorted(disabled_paths),
}
(backup / "codex-disable-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"Disabled {len(disabled_paths)} skill paths; TOML validated")
print(f"Config backup: {config_backup}")
