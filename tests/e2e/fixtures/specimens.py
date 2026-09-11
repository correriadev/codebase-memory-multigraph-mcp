"""
Source Code Specimens for Refactoring and Two-Tier Anchor E2E Simulations.
Physically manipulates code files on disk to test benign offset shifts vs breaking AST mutations.
"""

import os
from pathlib import Path
from typing import Tuple


# FNV-1a 64-bit hash matching C specification in src/admission/anchor_checker.c
FNV1A_64_OFFSET = 0xcbf29ce484222325
FNV1A_64_PRIME = 0x100000001b3
MASK_64 = 0xFFFFFFFFFFFFFFFF


def fnv1a_64(data: bytes) -> int:
    h = FNV1A_64_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV1A_64_PRIME) & MASK_64
    return h


class TwoTierRefactorSpecimen:
    """Manages a physical source file specimen on disk."""

    def __init__(self, project_dir: Path, rel_path: str = "src/calc.c"):
        self.project_dir = project_dir
        self.file_path = project_dir / rel_path
        self.file_path.parent.mkdir(parents=True, exist_ok=True)

        self.initial_code = (
            "#include <stdio.h>\n\n"
            "int add(int a, int b) {\n"
            "    return a + b;\n"
            "}\n"
        )
        self.target_symbol = "int add(int a, int b) {\n    return a + b;\n}"
        self.reset()

    def reset(self) -> None:
        """Writes the initial source code to disk."""
        self.file_path.write_text(self.initial_code, encoding="utf-8", newline="\n")

    @property
    def initial_anchor(self) -> Tuple[int, int, int]:
        """Returns (byte_start, byte_len, ast_signature_hash)."""
        content = self.initial_code.encode("utf-8")
        target = self.target_symbol.encode("utf-8")
        byte_start = content.find(target)
        byte_len = len(target)
        ast_hash = fnv1a_64(target)
        return byte_start, byte_len, ast_hash

    def apply_benign_comment_shift(self, comment_lines: int = 15) -> int:
        """
        Inserts comment lines at the beginning of the file.
        This shifts the byte offset of the target function without altering its AST.
        """
        comments = "".join([f"// Benign comment line #{i} header info\n" for i in range(comment_lines)])
        new_content = comments + "\n" + self.initial_code
        self.file_path.write_text(new_content, encoding="utf-8", newline="\n")
        return len(comments.encode("utf-8")) + 1

    def apply_breaking_mutation(self) -> None:
        """
        Modifies the function signature to double add(double a, double b, double c).
        This breaks both the byte offset and the AST signature.
        """
        mutated_code = (
            "#include <stdio.h>\n\n"
            "double add(double a, double b, double c) {\n"
            "    return a + b + c;\n"
            "}\n"
        )
        self.file_path.write_text(mutated_code, encoding="utf-8", newline="\n")
