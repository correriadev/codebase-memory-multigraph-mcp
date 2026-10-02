"""Compare parser encodings in the already-created real test horizon."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root / "tests/windows"))
from mcp_stdio import McpServer

directory = root / "docs/temenos-tests/real-20261001-190429"
report = json.loads((directory / "report.json").read_text(encoding="utf-8"))
markdown = (directory / "r001.md").read_text(encoding="utf-8")
graph = json.loads(markdown.split("```tactical-spec\n", 1)[1].split("\n```", 1)[0])
yaml = "nodes:\n"
for node in graph["nodes"]:
    yaml += "  - symbol: " + node["symbol"] + "\n"
    for key in ("type", "cbm_uri", "description"):
        yaml += "    " + key + ": '" + node[key].replace("'", "''") + "'\n"
yaml += "edges:\n"
for edge in graph["edges"]:
    yaml += "  - source: " + edge["source"] + "\n"
    for key in ("target", "type"):
        yaml += "    " + key + ": " + edge[key] + "\n"
events = []
with McpServer(str(Path.home() / ".local/bin/codebase-memory-mcp.exe"), cwd=str(root)) as client:
    client.initialize(timeout=40)
    for name, body in (("yaml", yaml), ("json-short", json.dumps({"nodes": [{"symbol": "D-03-PROBE", "type": "FractalDecision", "description": "Persistencia dos cards em Markdown, aceita pelo humano na sessao original; implementacao nao verificada."}], "edges": []}, indent=2))):
        text = "# Formato de memoria CBM\n\n## Grafo\n```tactical-spec\n" + body + "\n```\n"
        args = {"project": report["project"], "horizon_id": report["horizon_id"],
                "file_path": f"docs/temenos/{report['horizon_id']}/format-{name}.md", "content": text}
        response = client.call_tool("sync_horizon_spec", args, timeout=40)
        events.append({"format": name, "arguments": args, "response": response})
        print(name, client.tool_text(response)[0], flush=True)
(directory / "format-probe.json").write_text(json.dumps(events, ensure_ascii=False, indent=2), encoding="utf-8")
