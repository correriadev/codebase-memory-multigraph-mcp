"""Install reviewed workspace skills with verified backups and exact manifests."""
import hashlib
import json
import os
import shutil
import tempfile
from datetime import datetime
from pathlib import Path


def manifest(root):
    if root.is_symlink() or root.is_junction() or any(p.is_symlink() or p.is_junction() for p in root.rglob('*')):
        raise ValueError(f'Symlinks are not supported: {root}')
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file() and '__pycache__' not in p.parts}


def install():
    repo = Path(__file__).resolve().parents[1]
    sources = sorted([*(repo / 'skills').glob('*/SKILL.md'),
                      *(repo / 'skills-fractal').glob('*/SKILL.md')])
    if len(sources) != 11 or len({p.parent.name for p in sources}) != 11:
        raise ValueError('Expected eleven distinct skills')
    home = Path.home()
    roots = [Path(os.environ.get('CODEX_HOME', home / '.codex')) / 'skills',
             home / '.gemini/config/skills', home / '.gemini/antigravity-cli/skills',
             home / '.config/opencode/skills']
    backup = home / '.skill-backups' / ('cbm-reviewed-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True)
    records = []
    # Stage and verify all sources before touching installed skills.
    with tempfile.TemporaryDirectory(prefix='cbm-reviewed-') as stage_dir:
        stage = Path(stage_dir)
        for skill in sources:
            shutil.copytree(skill.parent, stage / skill.parent.name,
                            ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
            if manifest(skill.parent) != manifest(stage / skill.parent.name):
                raise ValueError('Staging mismatch')
        for root in roots:
            root.mkdir(parents=True, exist_ok=True)
            for skill in sources:
                source = stage / skill.parent.name
                target = root / skill.parent.name
                expected = manifest(source)
                if target.exists() and manifest(target) == expected:
                    records.append(dict(target=str(target), status='unchanged', hashes=expected))
                    continue
                saved = backup / str(len(records))
                if target.exists():
                    before = manifest(target)
                    shutil.copytree(target, saved)
                    if manifest(saved) != before:
                        raise ValueError(f'Backup mismatch: {target}')
                record = dict(target=str(target), backup=str(saved) if saved.exists() else None,
                              status='pending', hashes=expected)
                records.append(record)
                log = backup / 'manifest.json'
                log.write_text(json.dumps(records, indent=2), encoding='utf-8')
                # Move the old directory aside only after its backup is verified.
                old = target.with_name(target.name + '.cbm-install-old-' + backup.name)
                if target.exists():
                    target.rename(old)
                    record['previous_directory'] = str(old)
                try:
                    shutil.copytree(source, target)
                    if manifest(target) != expected:
                        raise ValueError(f'Installation mismatch: {target}')
                except Exception:
                    if target.exists():
                        shutil.move(str(target), str(backup / (str(len(records) - 1) + '-failed')))
                    if old.exists():
                        old.rename(target)
                    record['status'] = 'failed'
                    log.write_text(json.dumps(records, indent=2), encoding='utf-8')
                    raise
                # The old copy lives outside discovery after successful installation.
                if old.exists():
                    shutil.move(str(old), str(backup / (str(len(records) - 1) + '-original')))
                record['status'] = 'verified'
                log.write_text(json.dumps(records, indent=2), encoding='utf-8')
    (backup / 'manifest.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    print(json.dumps(dict(installations=len(records), backup=str(backup), roots=[str(p) for p in roots])))


if __name__ == '__main__':
    install()
