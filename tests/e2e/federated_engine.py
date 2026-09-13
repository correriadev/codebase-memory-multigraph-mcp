"""
Federated Query and K-Way Merge Engine for E2E Verification.
Implements the exact streaming K-Way merge with min-heap and shadowing
specified in src/query/kway_merge.c and src/mcp/handlers.c.
"""

import os
import heapq
import sqlite3
from pathlib import Path
from typing import Any, Dict, List, Optional


class FederatedRecord:
    def __init__(self, key: str, payload: Dict[str, Any], origin: str):
        self.key = key
        self.payload = payload
        self.origin = origin

    def __lt__(self, other: "FederatedRecord") -> bool:
        return self.key < other.key


def execute_federated_search(
    cache_dir: Path,
    project_name: str,
    pattern: str,
    active_horizons: Optional[List[str]] = None,
    limit: int = 100,
    offset: int = 0
) -> List[Dict[str, Any]]:
    """
    Executes a federated search across the Base Graph and active Horizon databases.
    Implements O(K) heap-based streaming merge with horizon node shadowing.
    """
    active_horizons = active_horizons or []
    cursors = []

    # 1. Base Graph Cursor (priority 0)
    base_db_path = cache_dir / f"{project_name}.db"
    if base_db_path.exists():
        conn = sqlite3.connect(base_db_path)
        cur = conn.cursor()
        query_sql = (
            "SELECT qualified_name, label, properties FROM nodes "
            "WHERE qualified_name LIKE ? OR name LIKE ? ORDER BY qualified_name ASC"
        )
        like_arg = f"%{pattern}%"
        cur.execute(query_sql, (like_arg, like_arg))
        cursors.append((0, "BASE", cur, conn))

    # 2. Active Horizon Cursors (priority 1..N: horizons shadow Base)
    for idx, h_id in enumerate(active_horizons, start=1):
        h_path = cache_dir / "horizons" / f"{h_id}.db"
        if h_path.exists():
            conn = sqlite3.connect(h_path)
            cur = conn.cursor()
            query_sql = (
                "SELECT cbm_uri, label, code_snippet AS properties FROM symbolic_nodes "
                "WHERE cbm_uri LIKE ? ORDER BY cbm_uri ASC"
            )
            like_arg = f"%{pattern}%"
            cur.execute(query_sql, (like_arg,))
            cursors.append((idx, h_id, cur, conn))

    # 3. K-Way Merge via Min-Heap (size <= K)
    # Priority tuple: (key, -priority) ensures that on identical keys, higher priority pops first
    heap = []
    for priority, origin, cur, _ in cursors:
        row = cur.fetchone()
        if row:
            key, label, props = row[0], row[1], row[2]
            heapq.heappush(heap, (key, -priority, origin, label, props, cur))

    emitted = []
    seen_keys = set()
    skipped = 0

    while heap:
        key, neg_prio, origin, label, props, cur = heapq.heappop(heap)

        # Advance cursor that produced this item
        next_row = cur.fetchone()
        if next_row:
            heapq.heappush(heap, (next_row[0], neg_prio, origin, next_row[1], next_row[2], cur))

        # Shadowing: if key has already been seen (emitted or shadowed by a higher-priority horizon),
        # skip duplicate. Horizon entries take precedence over Base entries.
        if key in seen_keys:
            continue
        seen_keys.add(key)

        # Pagination: apply offset
        if skipped < offset:
            skipped += 1
            continue

        # Pagination: apply limit
        if len(emitted) >= limit:
            break

        emitted.append({
            "key": key,
            "label": label,
            "properties": props,
            "origin": origin
        })

    # Close all connections
    for _, _, _, conn in cursors:
        conn.close()

    return emitted
