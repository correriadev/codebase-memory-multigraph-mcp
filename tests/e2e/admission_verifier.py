"""
Two-Tier Anchor Checker and Admission Gate Verifier for E2E Tests.
Implements the exact logic from src/admission/anchor_checker.c and src/admission/admission_gate.c.
"""

import os
import json
import sqlite3
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from tests.e2e.fixtures.specimens import fnv1a_64


class AdmissionStatus:
    ADMITTED = "ADMITTED"
    ANCHOR_DRIFT = "ANCHOR_DRIFT"
    INVARIANT_VIOLATION = "INVARIANT_VIOLATION"


def verify_two_tier_anchor(
    file_path: Path,
    byte_start: int,
    byte_len: int,
    expected_text: str,
    ast_signature_hash: int
) -> Tuple[bool, str]:
    """
    Two-Tier Anchor Verification:
    - Tier 1: Fast-path by byte offset.
    - Tier 2: AST signature match when offset has shifted.
    """
    if not file_path.exists():
        return False, "FILE_NOT_FOUND"

    content_bytes = file_path.read_bytes()
    file_size = len(content_bytes)

    # Tier 1: Fast offset check
    if byte_start + byte_len <= file_size:
        slice_bytes = content_bytes[byte_start:byte_start + byte_len]
        if slice_bytes == expected_text.encode("utf-8"):
            return True, "TIER_1_OFFSET_MATCH"

    # Tier 2: AST Signature match
    expected_bytes = expected_text.encode("utf-8")
    if expected_bytes in content_bytes:
        found_hash = fnv1a_64(expected_bytes)
        if found_hash == ast_signature_hash:
            return True, "TIER_2_AST_MATCH"

    return False, AdmissionStatus.ANCHOR_DRIFT


def execute_admission_promotion(
    base_db_path: Path,
    horizon_db_path: Path,
    project_name: str,
    anchors: List[Dict]
) -> Tuple[str, Optional[str]]:
    """
    Executes horizon promotion to Base Graph:
    1. Validates all anchors against disk files.
    2. If all valid, consolidates symbolic nodes to Base Graph and marks PROMOTED.
    3. If invalid, returns ANCHOR_DRIFT.
    """
    for a in anchors:
        passed, reason = verify_two_tier_anchor(
            file_path=Path(a["file_path"]),
            byte_start=a["byte_start"],
            byte_len=a["byte_len"],
            expected_text=a["expected_text"],
            ast_signature_hash=a["ast_signature_hash"]
        )
        if not passed:
            return AdmissionStatus.ANCHOR_DRIFT, f"Anchor verification failed: {reason}"

    # Consolidate into Base Graph
    h_conn = sqlite3.connect(horizon_db_path)
    b_conn = sqlite3.connect(base_db_path)
    try:
        # Read proposed nodes from horizon
        h_cur = h_conn.cursor()
        h_cur.execute("SELECT cbm_uri, label, properties FROM symbolic_nodes WHERE status = 'PROPOSED'")
        nodes_to_promote = h_cur.fetchall()

        # Insert into Base Graph
        for uri, label, props in nodes_to_promote:
            # Extract simple name from URI
            sym_name = uri.split("#")[-1] if "#" in uri else uri
            b_conn.execute(
                """INSERT OR REPLACE INTO nodes 
                   (project, label, name, qualified_name, file_path, properties)
                   VALUES (?, ?, ?, ?, ?, ?)""",
                (project_name, label, sym_name, uri, "", props)
            )

        # Update horizon status to PROMOTED
        h_conn.execute("UPDATE horizon_metadata SET status = 'PROMOTED'")
        h_conn.commit()
        b_conn.commit()
    finally:
        h_conn.close()
        b_conn.close()

    return AdmissionStatus.ADMITTED, None
