"""
Real-world Harness-Kit Specimen for E2E Two-Tier Anchor and Admission Gate Tests.
Replicates the physical TypeScript file layout from harness-kit.
"""

from pathlib import Path
from typing import Tuple
from tests.e2e.fixtures.specimens import fnv1a_64


class HarnessKitSpecimen:
    """Manages physical TypeScript files matching the harness-kit agent runner layout."""

    def __init__(self, project_dir: Path):
        self.project_dir = project_dir
        self.rel_path = "sdk/src/agent-runner/IAgentRunner.ts"
        self.file_path = project_dir / self.rel_path
        self.file_path.parent.mkdir(parents=True, exist_ok=True)

        self.initial_code = (
            "import type { AgentInvocation, AgentOutput, Runner } from './types'\n\n"
            "export interface IAgentRunner {\n"
            "  readonly type?: Runner\n"
            "  readonly writePromptToStdin?: boolean\n"
            "  run(invocation: AgentInvocation, options?: { signal?: AbortSignal }): Promise<AgentOutput>\n"
            "}\n"
        )
        self.target_symbol = "export interface IAgentRunner"
        self.reset()

    def reset(self) -> None:
        """Writes the initial TypeScript code to disk."""
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
        This shifts the byte offset of the target interface without altering its AST.
        """
        comments = "".join([f"// Benign comment header line #{i} for harness-kit\n" for i in range(comment_lines)])
        new_content = comments + "\n" + self.initial_code
        self.file_path.write_text(new_content, encoding="utf-8", newline="\n")
        return len(comments.encode("utf-8")) + 1

    def apply_breaking_mutation(self) -> None:
        """
        Modifies the interface declaration to breaking signature.
        This breaks both the byte offset and the AST signature.
        """
        mutated_code = (
            "import type { AgentInvocation, AgentOutput, Runner } from './types'\n\n"
            "export interface IDisjointRunner {\n"
            "  readonly type?: Runner\n"
            "  readonly writePromptToStdin?: boolean\n"
            "  execute(invocation: AgentInvocation): Promise<AgentOutput>\n"
            "}\n"
        )
        self.file_path.write_text(mutated_code, encoding="utf-8", newline="\n")
