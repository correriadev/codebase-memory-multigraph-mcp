"""Build a bounded tactical-spec envelope containing exact UTF-8 Markdown bytes."""
import argparse
import hashlib
import json
from pathlib import Path


def serialized(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def build(data, project, path, artifact_id, graph):
    text = data.decode("utf-8")  # Strict; preserve BOM and line endings.
    digest = hashlib.sha256(data).hexdigest()
    base = f"cbm://{project}/{path}#"
    nodes, edges, parts = [], [], []
    current = ""
    def payload(content, index):
        return {"artifact_id": artifact_id, "index": index,
                "sha256": hashlib.sha256(content.encode('utf-8')).hexdigest(),
                "content": content}
    def fits(content, index):
        description = serialized(payload(content, index))
        return len(('      "description": ' + serialized(description)).encode('utf-8')) <= 475
    for char in text:
        if current and not fits(current + char, len(parts)):
            parts.append(current)
            current = ""
        current += char
        if not fits(current, len(parts)):
            raise ValueError("A character cannot fit the parser line limit")
    if current or not parts:
        parts.append(current)
    root = {"artifact_id": artifact_id, "path": path, "sha256": digest,
            "byte_count": len(data), "part_count": len(parts), "join": "concat",
            "encoding": "utf-8", "line_endings": "preserved"}
    nodes.append({"symbol": artifact_id, "type": "FractalArtifact",
                  "cbm_uri": base + artifact_id, "description": serialized(root)})
    for index, content in enumerate(parts):
        symbol = f"{artifact_id}-P{index:05d}"
        nodes.append({"symbol": symbol, "type": "FractalArtifactPart",
                      "cbm_uri": base + symbol, "description": serialized(payload(content, index))})
        edges.append({"source": artifact_id, "target": symbol, "type": "CONTAINS_PART"})
        if index:
            edges.append({"source": f"{artifact_id}-P{index-1:05d}", "target": symbol, "type": "NEXT_PART"})
    nodes.extend(graph.get("nodes", []))
    edges.extend(graph.get("edges", []))
    symbols = [n['symbol'] for n in nodes]
    uris = [n['cbm_uri'] for n in nodes]
    if len(set(symbols)) != len(symbols) or len(set(uris)) != len(uris):
        raise ValueError("Duplicate symbols or URIs")
    for node in nodes:
        if len(node['description'].encode('utf-8')) >= 3000:
            raise ValueError("Oversized payload: " + node['symbol'])
    encoded = json.dumps({"nodes": nodes, "edges": edges}, ensure_ascii=False, indent=2)
    if any(len(line.encode('utf-8')) > 480 for line in encoded.splitlines()):
        raise ValueError("JSON line exceeds 480 UTF-8 bytes; split semantic payloads without truncation")
    envelope = "# Generated CBM transport\n\n```tactical-spec\n" + encoded + "\n```\n"
    assert ''.join(parts).encode('utf-8') == data
    return envelope, {"artifact_id": artifact_id, "sha256": digest,
                      "byte_count": len(data), "part_count": len(parts),
                      "expected_nodes": len(nodes), "expected_edges": len(edges)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True)
    parser.add_argument('--graph', required=True)
    parser.add_argument('--project', required=True)
    parser.add_argument('--path', required=True)
    parser.add_argument('--artifact-id', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    envelope, manifest = build(Path(args.input).read_bytes(), args.project, args.path,
                               args.artifact_id, json.loads(Path(args.graph).read_text(encoding='utf-8')))
    Path(args.output).write_bytes(envelope.encode('utf-8'))
    print(json.dumps(manifest, ensure_ascii=True))
