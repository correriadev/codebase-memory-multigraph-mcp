#!/usr/bin/env python3
"""
Test suite for Feature F001 (Tree-sitter AST Anchor Isolation).
Translates all Given-When-Then scenarios from:
docs/specs/ast_anchor_isolation/004-codebase-memory-multigraph-mcp-test-scenarios.md
"""

import os
import sys
import ctypes
import tempfile
import sqlite3
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# CTypes definitions matching anchor_checker.h and tree_sitter/api.h
# ---------------------------------------------------------------------------

class TwoTierAnchor(ctypes.Structure):
    _fields_ = [
        ("file_path", ctypes.c_char * 512),
        ("symbol_name", ctypes.c_char * 256),
        ("byte_start", ctypes.c_uint32),
        ("byte_len", ctypes.c_uint32),
        ("ast_signature_hash", ctypes.c_uint64),
        ("expected_text", ctypes.c_char * 1024),
    ]

class TwoTierAnchorRelocation(ctypes.Structure):
    _fields_ = [
        ("new_byte_start", ctypes.c_uint32),
        ("new_byte_len", ctypes.c_uint32),
        ("was_relocated", ctypes.c_bool),
        ("structural_hash_matched", ctypes.c_bool),
    ]

class TSNode(ctypes.Structure):
    _fields_ = [
        ("context", ctypes.c_uint32 * 4),
        ("id", ctypes.c_void_p),
        ("tree", ctypes.c_void_p),
    ]

class DeclarationNodeMatch(ctypes.Structure):
    _fields_ = [
        ("node", TSNode),
        ("start_byte", ctypes.c_uint32),
        ("end_byte", ctypes.c_uint32),
        ("node_type", ctypes.c_char_p),
    ]

CBM_LANG_UNKNOWN = 0
CBM_LANG_C = 1
CBM_LANG_TYPESCRIPT = 2
CBM_LANG_PYTHON = 3

def get_cbm_dll():
    dll_path = REPO_ROOT / "build" / "libcbm_anchor.dll"
    srcs = [
        REPO_ROOT / "src" / "core" / "cbm_uri.c",
        REPO_ROOT / "src" / "admission" / "anchor_checker.c",
        REPO_ROOT / "src" / "admission" / "anchor_checker.h",
    ]
    recompile = not dll_path.exists() or any(s.stat().st_mtime > dll_path.stat().st_mtime for s in srcs)
    if recompile:
        # Compile DLL on the fly if needed
        import subprocess
        env = os.environ.copy()
        env["PATH"] = r"C:\msys64\clang64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")
        clang_bin = r"C:\msys64\clang64\bin\clang.exe" if os.path.exists(r"C:\msys64\clang64\bin\clang.exe") else "clang"
        cmd = [
            clang_bin, "-shared", "-fPIC", "-Wl,--export-all-symbols",
            str(REPO_ROOT / "src" / "core" / "cbm_uri.c"),
            str(REPO_ROOT / "src" / "admission" / "anchor_checker.c"),
            str(REPO_ROOT / "internal" / "cbm" / "ts_runtime.c"),
            str(REPO_ROOT / "internal" / "cbm" / "grammar_c.c"),
            str(REPO_ROOT / "internal" / "cbm" / "grammar_typescript.c"),
            str(REPO_ROOT / "internal" / "cbm" / "grammar_python.c"),
            f"-I{REPO_ROOT / 'src'}",
            f"-I{REPO_ROOT / 'internal' / 'cbm'}",
            f"-I{REPO_ROOT / 'internal' / 'cbm' / 'vendored' / 'ts_runtime' / 'include'}",
            f"-I{REPO_ROOT / 'internal' / 'cbm' / 'vendored' / 'ts_runtime' / 'src'}",
            "-o", str(dll_path)
        ]
        subprocess.check_call(cmd, env=env)

    dll = ctypes.CDLL(str(dll_path))

    dll.cbm_resolve_language_from_path.argtypes = [ctypes.c_char_p]
    dll.cbm_resolve_language_from_path.restype = ctypes.c_int

    dll.cbm_fast_offset_match.argtypes = [ctypes.c_char_p, ctypes.POINTER(TwoTierAnchor)]
    dll.cbm_fast_offset_match.restype = ctypes.c_bool

    dll.cbm_verify_two_tier_anchor.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(TwoTierAnchor),
        ctypes.POINTER(ctypes.c_bool),
        ctypes.POINTER(TwoTierAnchorRelocation)
    ]
    dll.cbm_verify_two_tier_anchor.restype = ctypes.c_int

    dll.cbm_ast_signature_match.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(TwoTierAnchor),
        ctypes.POINTER(ctypes.c_bool)
    ]
    dll.cbm_ast_signature_match.restype = ctypes.c_int

    dll.ts_parser_new.restype = ctypes.c_void_p
    dll.ts_parser_delete.argtypes = [ctypes.c_void_p]
    dll.ts_parser_set_language.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    dll.ts_parser_parse_string.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32]
    dll.ts_parser_parse_string.restype = ctypes.c_void_p
    dll.ts_tree_delete.argtypes = [ctypes.c_void_p]
    dll.ts_tree_root_node.argtypes = [ctypes.c_void_p]
    dll.ts_tree_root_node.restype = TSNode

    dll.tree_sitter_c.restype = ctypes.c_void_p
    dll.tree_sitter_typescript.restype = ctypes.c_void_p
    dll.tree_sitter_python.restype = ctypes.c_void_p

    dll.cbm_ts_find_declaration_node.argtypes = [
        TSNode,
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.POINTER(DeclarationNodeMatch)
    ]
    dll.cbm_ts_find_declaration_node.restype = ctypes.c_bool

    dll.cbm_ts_compute_declaration_hash.argtypes = [TSNode, ctypes.c_char_p]
    dll.cbm_ts_compute_declaration_hash.restype = ctypes.c_uint64

    return dll


class TestAstAnchorIsolationScenarios(unittest.TestCase):
    """Executes the test scenarios from 004-codebase-memory-multigraph-mcp-test-scenarios.md"""

    @classmethod
    def setUpClass(cls):
        cls.cbm = get_cbm_dll()

    # =========================================================================
    # Section 1 — Unit Tests
    # =========================================================================

    # --- 1.1 Value Objects and Struct Contracts ---

    def test_relocation_default_clean_state(self):
        """Should initialize TwoTierAnchorRelocation with default clean state"""
        reloc = TwoTierAnchorRelocation()
        self.assertFalse(reloc.was_relocated)
        self.assertFalse(reloc.structural_hash_matched)
        self.assertEqual(reloc.new_byte_start, 0)
        self.assertEqual(reloc.new_byte_len, 0)

    def test_resolve_language_c_and_header(self):
        """Should resolve CBM_LANG_C when given C and Header file extensions"""
        res_c = self.cbm.cbm_resolve_language_from_path(b"src/admission/anchor_checker.c")
        res_h = self.cbm.cbm_resolve_language_from_path(b"src/admission/anchor_checker.h")
        self.assertEqual(res_c, CBM_LANG_C)
        self.assertEqual(res_h, CBM_LANG_C)

    def test_resolve_language_typescript_and_javascript(self):
        """Should resolve CBM_LANG_TYPESCRIPT when given TypeScript and JavaScript extensions"""
        res_ts = self.cbm.cbm_resolve_language_from_path(b"sdk/src/runner.ts")
        res_js = self.cbm.cbm_resolve_language_from_path(b"lib/utils.js")
        res_tsx = self.cbm.cbm_resolve_language_from_path(b"components/app.tsx")
        res_jsx = self.cbm.cbm_resolve_language_from_path(b"views/view.jsx")
        self.assertEqual(res_ts, CBM_LANG_TYPESCRIPT)
        self.assertEqual(res_js, CBM_LANG_TYPESCRIPT)
        self.assertEqual(res_tsx, CBM_LANG_TYPESCRIPT)
        self.assertEqual(res_jsx, CBM_LANG_TYPESCRIPT)

    def test_resolve_language_python(self):
        """Should resolve CBM_LANG_PYTHON when given Python file extensions"""
        res_py = self.cbm.cbm_resolve_language_from_path(b"tests/harness/verifier.py")
        self.assertEqual(res_py, CBM_LANG_PYTHON)

    def test_resolve_language_unknown(self):
        """Should return CBM_LANG_UNKNOWN when given unsupported extensions"""
        res_md = self.cbm.cbm_resolve_language_from_path(b"docs/README.md")
        res_make = self.cbm.cbm_resolve_language_from_path(b"Makefile")
        res_none = self.cbm.cbm_resolve_language_from_path(None)
        res_empty = self.cbm.cbm_resolve_language_from_path(b"")
        self.assertEqual(res_md, CBM_LANG_UNKNOWN)
        self.assertEqual(res_make, CBM_LANG_UNKNOWN)
        self.assertEqual(res_none, CBM_LANG_UNKNOWN)
        self.assertEqual(res_empty, CBM_LANG_UNKNOWN)

    # --- 1.2 Domain Services: Tree-sitter AST Navigation and Hashing ---

    def test_find_declaration_node_ignores_comment_block(self):
        """Should locate live function_definition node and ignore occurrence in comment block"""
        code = (
            b"/* int compute_total(void) { return 0; } */\n"
            b"// comment line\n"
            + b"\n" * 20
            + b"int compute_total(void) { return 42; }\n"
        )
        p = self.cbm.ts_parser_new()
        try:
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
            tree = self.cbm.ts_parser_parse_string(p, None, code, len(code))
            try:
                root = self.cbm.ts_tree_root_node(tree)
                match = DeclarationNodeMatch()
                found = self.cbm.cbm_ts_find_declaration_node(root, b"compute_total", code, ctypes.byref(match))
                self.assertTrue(found, "Must find live declaration")
                self.assertEqual(match.node_type, b"function_definition")
                # Ensure start_byte points to the live function, not the comment at offset 0
                self.assertGreater(match.start_byte, 50)
                slice_code = code[match.start_byte:match.end_byte]
                self.assertIn(b"return 42", slice_code)
                self.assertNotIn(b"return 0", slice_code)
            finally:
                self.cbm.ts_tree_delete(tree)
        finally:
            self.cbm.ts_parser_delete(p)

    def test_find_declaration_node_typescript_interface_or_class(self):
        """Should locate live class_declaration or interface_declaration in TypeScript"""
        code = b"export interface IAgentRunner { run(): void; }\n"
        p = self.cbm.ts_parser_new()
        try:
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_typescript())
            tree = self.cbm.ts_parser_parse_string(p, None, code, len(code))
            try:
                root = self.cbm.ts_tree_root_node(tree)
                match = DeclarationNodeMatch()
                found = self.cbm.cbm_ts_find_declaration_node(root, b"IAgentRunner", code, ctypes.byref(match))
                self.assertTrue(found)
                self.assertEqual(match.node_type, b"interface_declaration")
                self.assertEqual(code[match.start_byte:match.end_byte], b"interface IAgentRunner { run(): void; }")
            finally:
                self.cbm.ts_tree_delete(tree)
        finally:
            self.cbm.ts_parser_delete(p)

    def test_find_declaration_node_rejects_string_literal(self):
        """Should reject declaration search when symbol only exists inside a string literal"""
        code = b'int main(void) { printf("Starting process_payment..."); return 0; }\n'
        p = self.cbm.ts_parser_new()
        try:
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
            tree = self.cbm.ts_parser_parse_string(p, None, code, len(code))
            try:
                root = self.cbm.ts_tree_root_node(tree)
                match = DeclarationNodeMatch()
                found = self.cbm.cbm_ts_find_declaration_node(root, b"process_payment", code, ctypes.byref(match))
                self.assertFalse(found, "String literal occurrence must not be identified as declaration")
                self.assertIsNone(match.node.id, "Node ID must be NULL on failed match")
            finally:
                self.cbm.ts_tree_delete(tree)
        finally:
            self.cbm.ts_parser_delete(p)

    def test_compute_declaration_hash_identical_with_comments(self):
        """Should compute identical structural hash despite internal comment modifications"""
        src_original = b"int compute_total(void) {\n    return 42;\n}\n"
        src_with_comments = (
            b"int compute_total(void) {\n"
            b"    // Explanation comment 1\n"
            b"    /* Detailed block comment 2 */\n"
            b"    // Trailing note 3\n"
            b"    return 42;\n"
            b"}\n"
        )
        p = self.cbm.ts_parser_new()
        try:
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
            t1 = self.cbm.ts_parser_parse_string(p, None, src_original, len(src_original))
            t2 = self.cbm.ts_parser_parse_string(p, None, src_with_comments, len(src_with_comments))
            try:
                m1 = DeclarationNodeMatch()
                m2 = DeclarationNodeMatch()
                self.assertTrue(self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t1), b"compute_total", src_original, ctypes.byref(m1)))
                self.assertTrue(self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t2), b"compute_total", src_with_comments, ctypes.byref(m2)))

                h1 = self.cbm.cbm_ts_compute_declaration_hash(m1.node, src_original)
                h2 = self.cbm.cbm_ts_compute_declaration_hash(m2.node, src_with_comments)
                self.assertNotEqual(h1, 0)
                self.assertEqual(h1, h2, "Structural hash must remain identical when comment trivia is inserted")
            finally:
                self.cbm.ts_tree_delete(t1)
                self.cbm.ts_tree_delete(t2)
        finally:
            self.cbm.ts_parser_delete(p)

    def test_compute_declaration_hash_diverges_on_semantic_change(self):
        """Should compute different structural hash when function signature or statements change"""
        src_original = b"int compute_total(void) {\n    return 42;\n}\n"
        src_diff_type = b"double compute_total(void) {\n    return 42;\n}\n"
        src_diff_body = b"int compute_total(void) {\n    return 100;\n}\n"

        p = self.cbm.ts_parser_new()
        try:
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
            t1 = self.cbm.ts_parser_parse_string(p, None, src_original, len(src_original))
            t2 = self.cbm.ts_parser_parse_string(p, None, src_diff_type, len(src_diff_type))
            t3 = self.cbm.ts_parser_parse_string(p, None, src_diff_body, len(src_diff_body))
            try:
                m1 = DeclarationNodeMatch()
                m2 = DeclarationNodeMatch()
                m3 = DeclarationNodeMatch()
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t1), b"compute_total", src_original, ctypes.byref(m1))
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t2), b"compute_total", src_diff_type, ctypes.byref(m2))
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t3), b"compute_total", src_diff_body, ctypes.byref(m3))

                h1 = self.cbm.cbm_ts_compute_declaration_hash(m1.node, src_original)
                h2 = self.cbm.cbm_ts_compute_declaration_hash(m2.node, src_diff_type)
                h3 = self.cbm.cbm_ts_compute_declaration_hash(m3.node, src_diff_body)

                self.assertNotEqual(h1, h2, "Hash must diverge when return type changes")
                self.assertNotEqual(h1, h3, "Hash must diverge when return statement value changes")
            finally:
                self.cbm.ts_tree_delete(t1)
                self.cbm.ts_tree_delete(t2)
                self.cbm.ts_tree_delete(t3)
        finally:
            self.cbm.ts_parser_delete(p)

    # =========================================================================
    # Section 2 — Integration Tests
    # =========================================================================

    # --- 2.1 Two-Tier Verifier Integration (cbm_verify_two_tier_anchor) ---

    def test_verify_two_tier_tier1_fast_path(self):
        """Should admit anchor via Tier 1 fast-path when byte span and AST hash match"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "math_util.c"
            full_path = Path(td) / file_name
            header = "// Header comment\n" * 5
            body = "int add(int a, int b) { return a + b; }\n"
            content = (header + body).encode("utf-8")
            full_path.write_bytes(content)

            byte_start = len(header.encode("utf-8"))
            byte_len = len(body.encode("utf-8"))

            p = self.cbm.ts_parser_new()
            try:
                self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
                t = self.cbm.ts_parser_parse_string(p, None, body.encode("utf-8"), len(body))
                try:
                    m = DeclarationNodeMatch()
                    self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"add", body.encode("utf-8"), ctypes.byref(m))
                    real_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, body.encode("utf-8"))
                finally:
                    self.cbm.ts_tree_delete(t)
            finally:
                self.cbm.ts_parser_delete(p)

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"add"
            anchor.byte_start = byte_start
            anchor.byte_len = byte_len
            anchor.expected_text = body.encode("utf-8")
            anchor.ast_signature_hash = real_hash

            ok = ctypes.c_bool(False)
            rc = self.cbm.cbm_verify_two_tier_anchor(td.encode("utf-8"), ctypes.byref(anchor), ctypes.byref(ok), None)

            self.assertEqual(rc, 0)
            self.assertTrue(ok.value, "Tier 1 fast path must succeed for unmodified byte span with matching AST hash")

    def test_verify_two_tier_rejects_comment_with_fictitious_hash_codex_counterexample(self):
        """Should reject comment span with matching bytes but fictitious hash or non-declaration symbol (Codex Finding)"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "comment_sample.c"
            full_path = Path(td) / file_name
            comment_text = "// This is just a comment line\n"
            content = comment_text.encode("utf-8")
            full_path.write_bytes(content)

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"NONEXISTENT"
            anchor.byte_start = 0
            anchor.byte_len = len(content)
            anchor.expected_text = content
            anchor.ast_signature_hash = 1  # Fictitious hash

            # Direct fast-path check
            fast_match = self.cbm.cbm_fast_offset_match(td.encode("utf-8"), ctypes.byref(anchor))
            self.assertTrue(fast_match, "Fast path byte match succeeds on exact byte match")

            # Two-tier anchor verifier check
            ok = ctypes.c_bool(False)
            rc = self.cbm.cbm_verify_two_tier_anchor(td.encode("utf-8"), ctypes.byref(anchor), ctypes.byref(ok), None)
            self.assertEqual(rc, 0)
            self.assertFalse(ok.value, "Two-tier verifier must reject comment span with fictitious hash")

    def test_verify_two_tier_rejects_string_literal_codex_counterexample(self):
        """Should reject anchor pointing to function code inside a string literal (Codex Finding test_anchor_checker.c:140)"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "string_sample.c"
            full_path = Path(td) / file_name
            literal_code = "int dummy(void) { return 0; }"
            file_content = f'const char *code = "{literal_code}";\n'.encode("utf-8")
            full_path.write_bytes(file_content)

            # Find byte start of literal_code inside file_content
            byte_start = file_content.index(literal_code.encode("utf-8"))
            byte_len = len(literal_code.encode("utf-8"))

            # Compute hash of the function definition if it were parsed in isolation
            p = self.cbm.ts_parser_new()
            try:
                self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
                t = self.cbm.ts_parser_parse_string(p, None, literal_code.encode("utf-8"), byte_len)
                try:
                    m = DeclarationNodeMatch()
                    self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"dummy", literal_code.encode("utf-8"), ctypes.byref(m))
                    ast_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, literal_code.encode("utf-8"))
                finally:
                    self.cbm.ts_tree_delete(t)
            finally:
                self.cbm.ts_parser_delete(p)

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"dummy"
            anchor.byte_start = byte_start
            anchor.byte_len = byte_len
            anchor.expected_text = literal_code.encode("utf-8")
            anchor.ast_signature_hash = ast_hash

            # Byte matching directly succeeds because byte contents match
            fast_match = self.cbm.cbm_fast_offset_match(td.encode("utf-8"), ctypes.byref(anchor))
            self.assertTrue(fast_match)

            # BUT Two-Tier Anchor verifier MUST REJECT because "dummy" is inside a string literal in file AST
            ok = ctypes.c_bool(True)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )
            self.assertEqual(rc, 0)
            self.assertFalse(ok.value, "Must reject anchor pointing to declaration inside string literal")
            self.assertFalse(reloc.was_relocated)

    def test_verify_two_tier_tier2_benign_comment_shift(self):
        """Should admit anchor and populate relocation coordinates on benign comment shift"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "processor.py"
            full_path = Path(td) / file_name

            func_text = "def process_data(items):\n    return [x * 2 for x in items]\n"
            full_path.write_bytes(func_text.encode("utf-8"))

            # Determine AST signature hash of original definition
            p = self.cbm.ts_parser_new()
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_python())
            t = self.cbm.ts_parser_parse_string(p, None, func_text.encode("utf-8"), len(func_text))
            m = DeclarationNodeMatch()
            self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"process_data", func_text.encode("utf-8"), ctypes.byref(m))
            orig_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, func_text.encode("utf-8"))
            self.cbm.ts_tree_delete(t)
            self.cbm.ts_parser_delete(p)

            # Prepend 15 comment lines (approx 350 bytes)
            comment_prefix = "\n".join([f"# Header comment line {i:02d} with padding metadata" for i in range(15)]) + "\n\n"
            shifted_content = comment_prefix + func_text
            full_path.write_bytes(shifted_content.encode("utf-8"))

            shift_bytes = len(comment_prefix.encode("utf-8"))

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"process_data"
            anchor.byte_start = 0  # Old position before shift
            anchor.byte_len = len(func_text.encode("utf-8"))
            anchor.expected_text = func_text.encode("utf-8")
            anchor.ast_signature_hash = orig_hash

            ok = ctypes.c_bool(False)
            reloc = TwoTierAnchorRelocation()

            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )

            self.assertEqual(rc, 0)
            self.assertTrue(ok.value, "Tier 2 AST fallback must admit benign shift")
            self.assertTrue(reloc.was_relocated, "was_relocated must be true")
            self.assertTrue(reloc.structural_hash_matched, "structural_hash_matched must be true")
            self.assertEqual(reloc.new_byte_start, shift_bytes, "new_byte_start must match shift offset")
            self.assertEqual(reloc.new_byte_len, len(func_text.encode("utf-8").strip()))

    def test_verify_two_tier_backward_compat_null_relocation(self):
        """Should maintain backward compatibility when caller passes NULL for out_relocation"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "handler.ts"
            full_path = Path(td) / file_name

            code = "export class TokenHandler {\n    validate() { return true; }\n}\n"
            # AST hash
            p = self.cbm.ts_parser_new()
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_typescript())
            t = self.cbm.ts_parser_parse_string(p, None, code.encode("utf-8"), len(code))
            m = DeclarationNodeMatch()
            self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"TokenHandler", code.encode("utf-8"), ctypes.byref(m))
            orig_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, code.encode("utf-8"))
            self.cbm.ts_tree_delete(t)
            self.cbm.ts_parser_delete(p)

            # Shift with comment
            shifted = "// License: MIT\n// Author: Antigravity\n" + code
            full_path.write_bytes(shifted.encode("utf-8"))

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"TokenHandler"
            anchor.byte_start = 0
            anchor.byte_len = len(code.encode("utf-8"))
            anchor.expected_text = code.encode("utf-8")
            anchor.ast_signature_hash = orig_hash

            ok = ctypes.c_bool(False)
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                None  # NULL relocation pointer
            )

            self.assertEqual(rc, 0)
            self.assertTrue(ok.value, "Must succeed without segmentation fault when relocation pointer is NULL")

    def test_verify_two_tier_refuses_modified_code_anchor_drift(self):
        """Should refuse admission with ANCHOR_DRIFT when code was modified"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "calc.c"
            full_path = Path(td) / file_name

            orig = "int calculate(int x) { return x * 2; }\n"
            modified = "// header\nint calculate(int x, int y) { return x * y; }\n"
            full_path.write_bytes(modified.encode("utf-8"))

            # Hash of original
            p = self.cbm.ts_parser_new()
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
            t = self.cbm.ts_parser_parse_string(p, None, orig.encode("utf-8"), len(orig))
            m = DeclarationNodeMatch()
            self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"calculate", orig.encode("utf-8"), ctypes.byref(m))
            orig_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, orig.encode("utf-8"))
            self.cbm.ts_tree_delete(t)
            self.cbm.ts_parser_delete(p)

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"calculate"
            anchor.byte_start = 0
            anchor.byte_len = len(orig.encode("utf-8"))
            anchor.expected_text = orig.encode("utf-8")
            anchor.ast_signature_hash = orig_hash

            ok = ctypes.c_bool(True)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )

            self.assertEqual(rc, 0)
            self.assertFalse(ok.value, "Verification must fail when AST signature has drifted")
            self.assertFalse(reloc.was_relocated)

    def test_verify_two_tier_refuses_missing_symbol(self):
        """Should refuse admission when symbol was deleted from file"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "service.c"
            full_path = Path(td) / file_name
            full_path.write_bytes(b"int other_function(void) { return 0; }\n")

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"deleted_symbol"
            anchor.byte_start = 0
            anchor.byte_len = 10
            anchor.expected_text = b"int deleted"
            anchor.ast_signature_hash = 999999

            ok = ctypes.c_bool(True)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )

            self.assertEqual(rc, 0)
            self.assertFalse(ok.value, "Verification must fail when symbol does not exist in AST")

    # =========================================================================
    # Section 3 — Functional and Acceptance Scenarios
    # =========================================================================

    # --- 3.1 End-to-End Promotion and Relocation Lifecycle ---

    def test_e2e_prevents_false_success_when_old_code_in_comment_codex_a03(self):
        """Should prevent false success when old function text is left in a comment (Codex Finding A03)"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "tax.c"
            full_path = Path(td) / file_name

            old_decl = "double calculate_tax(double amount) { return amount * 0.15; }"
            new_decl = "double calculate_tax(double amount, double rate) { return amount * rate; }"

            # File has old declaration pasted in comments, but live declaration has drifted
            file_content = (
                f"/* Old implementation backup:\n"
                f" * {old_decl}\n"
                f" */\n\n"
                f"{new_decl}\n"
            )
            full_path.write_bytes(file_content.encode("utf-8"))

            # Original expected hash
            p = self.cbm.ts_parser_new()
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
            t = self.cbm.ts_parser_parse_string(p, None, old_decl.encode("utf-8"), len(old_decl))
            m = DeclarationNodeMatch()
            self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"calculate_tax", old_decl.encode("utf-8"), ctypes.byref(m))
            old_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, old_decl.encode("utf-8"))
            self.cbm.ts_tree_delete(t)
            self.cbm.ts_parser_delete(p)

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"calculate_tax"
            anchor.byte_start = 1000  # Offset in an ephemeral proposal that now misses
            anchor.byte_len = len(old_decl.encode("utf-8"))
            anchor.expected_text = old_decl.encode("utf-8")
            anchor.ast_signature_hash = old_hash

            ok = ctypes.c_bool(True)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )

            self.assertEqual(rc, 0)
            self.assertFalse(ok.value, "Codex Finding A03: checker MUST NOT match text in comment; must detect live drift")
            self.assertFalse(reloc.was_relocated)

    def test_e2e_multi_file_promotion_mixed_exact_and_shifted(self):
        """Should handle multi-file promotion with mixed exact matches and benign shifts"""
        with tempfile.TemporaryDirectory() as td:
            # File A: Exact match
            code_a = "int func_a(void) { return 10; }\n"
            path_a = Path(td) / "a.c"
            path_a.write_bytes(code_a.encode("utf-8"))

            # File B: Shifted with comments
            code_b = "int func_b(void) { return 20; }\n"
            path_b = Path(td) / "b.c"
            shift_header = "// 10 lines of license header\n" * 10
            path_b.write_bytes((shift_header + code_b).encode("utf-8"))

            # File C: Exact match
            code_c = "int func_c(void) { return 30; }\n"
            path_c = Path(td) / "c.c"
            path_c.write_bytes(code_c.encode("utf-8"))

            # Hash for A, B, C
            p = self.cbm.ts_parser_new()
            try:
                self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
                t_a = self.cbm.ts_parser_parse_string(p, None, code_a.encode("utf-8"), len(code_a))
                m_a = DeclarationNodeMatch()
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t_a), b"func_a", code_a.encode("utf-8"), ctypes.byref(m_a))
                hash_a = self.cbm.cbm_ts_compute_declaration_hash(m_a.node, code_a.encode("utf-8"))
                self.cbm.ts_tree_delete(t_a)

                t_b = self.cbm.ts_parser_parse_string(p, None, code_b.encode("utf-8"), len(code_b))
                m_b = DeclarationNodeMatch()
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t_b), b"func_b", code_b.encode("utf-8"), ctypes.byref(m_b))
                hash_b = self.cbm.cbm_ts_compute_declaration_hash(m_b.node, code_b.encode("utf-8"))
                self.cbm.ts_tree_delete(t_b)

                t_c = self.cbm.ts_parser_parse_string(p, None, code_c.encode("utf-8"), len(code_c))
                m_c = DeclarationNodeMatch()
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t_c), b"func_c", code_c.encode("utf-8"), ctypes.byref(m_c))
                hash_c = self.cbm.cbm_ts_compute_declaration_hash(m_c.node, code_c.encode("utf-8"))
                self.cbm.ts_tree_delete(t_c)
            finally:
                self.cbm.ts_parser_delete(p)

            # Build anchors
            anchor_a = TwoTierAnchor()
            anchor_a.file_path = b"a.c"
            anchor_a.symbol_name = b"func_a"
            anchor_a.byte_start = 0
            anchor_a.byte_len = len(code_a.encode("utf-8"))
            anchor_a.expected_text = code_a.encode("utf-8")
            anchor_a.ast_signature_hash = hash_a

            anchor_b = TwoTierAnchor()
            anchor_b.file_path = b"b.c"
            anchor_b.symbol_name = b"func_b"
            anchor_b.byte_start = 0  # pre-shift offset
            anchor_b.byte_len = len(code_b.encode("utf-8"))
            anchor_b.expected_text = code_b.encode("utf-8")
            anchor_b.ast_signature_hash = hash_b

            anchor_c = TwoTierAnchor()
            anchor_c.file_path = b"c.c"
            anchor_c.symbol_name = b"func_c"
            anchor_c.byte_start = 0
            anchor_c.byte_len = len(code_c.encode("utf-8"))
            anchor_c.expected_text = code_c.encode("utf-8")
            anchor_c.ast_signature_hash = hash_c

            anchors = [anchor_a, anchor_b, anchor_c]
            all_passed = True
            b_relocated = False

            for anc in anchors:
                ok = ctypes.c_bool(False)
                reloc = TwoTierAnchorRelocation()
                rc = self.cbm.cbm_verify_two_tier_anchor(
                    td.encode("utf-8"),
                    ctypes.byref(anc),
                    ctypes.byref(ok),
                    ctypes.byref(reloc)
                )
                if rc != 0 or not ok.value:
                    all_passed = False
                    break
                if anc.symbol_name == b"func_b" and reloc.was_relocated:
                    b_relocated = True
                    anc.byte_start = reloc.new_byte_start
                    anc.byte_len = reloc.new_byte_len

            self.assertTrue(all_passed, "All anchors in proposal must pass verification")
            self.assertTrue(b_relocated, "Anchor B must record relocation")
            self.assertEqual(anchor_b.byte_start, len(shift_header.encode("utf-8")))

    def test_e2e_rejects_atomically_if_any_anchor_drifts(self):
        """Should reject entire promotion atomically if any anchor in the set suffers semantic drift"""
        with tempfile.TemporaryDirectory() as td:
            # File 1 & 2 valid
            (Path(td) / "1.c").write_bytes(b"int f1(void) { return 1; }\n")
            (Path(td) / "2.c").write_bytes(b"int f2(void) { return 2; }\n")
            # File 3 drifted
            (Path(td) / "3.c").write_bytes(b"int f3(int drift) { return drift; }\n")

            p = self.cbm.ts_parser_new()
            try:
                self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
                t1 = self.cbm.ts_parser_parse_string(p, None, b"int f1(void) { return 1; }\n", len("int f1(void) { return 1; }\n"))
                m1 = DeclarationNodeMatch()
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t1), b"f1", b"int f1(void) { return 1; }\n", ctypes.byref(m1))
                h1 = self.cbm.cbm_ts_compute_declaration_hash(m1.node, b"int f1(void) { return 1; }\n")
                self.cbm.ts_tree_delete(t1)

                t2 = self.cbm.ts_parser_parse_string(p, None, b"int f2(void) { return 2; }\n", len("int f2(void) { return 2; }\n"))
                m2 = DeclarationNodeMatch()
                self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t2), b"f2", b"int f2(void) { return 2; }\n", ctypes.byref(m2))
                h2 = self.cbm.cbm_ts_compute_declaration_hash(m2.node, b"int f2(void) { return 2; }\n")
                self.cbm.ts_tree_delete(t2)
            finally:
                self.cbm.ts_parser_delete(p)

            a1 = TwoTierAnchor()
            a1.file_path = b"1.c"
            a1.symbol_name = b"f1"
            a1.byte_start = 0
            a1.byte_len = len("int f1(void) { return 1; }\n")
            a1.expected_text = b"int f1(void) { return 1; }\n"
            a1.ast_signature_hash = h1

            a2 = TwoTierAnchor()
            a2.file_path = b"2.c"
            a2.symbol_name = b"f2"
            a2.byte_start = 0
            a2.byte_len = len("int f2(void) { return 2; }\n")
            a2.expected_text = b"int f2(void) { return 2; }\n"
            a2.ast_signature_hash = h2

            a3 = TwoTierAnchor()
            a3.file_path = b"3.c"
            a3.symbol_name = b"f3"
            a3.byte_start = 0
            a3.byte_len = 20
            a3.expected_text = b"int f3(void) { return 3; }"
            a3.ast_signature_hash = 99999999  # Old hash; diverged

            anchors = [a1, a2, a3]
            promotion_admitted = True

            for anc in anchors:
                ok = ctypes.c_bool(False)
                rc = self.cbm.cbm_verify_two_tier_anchor(
                    td.encode("utf-8"),
                    ctypes.byref(anc),
                    ctypes.byref(ok),
                    None
                )
                if rc != 0 or not ok.value:
                    promotion_admitted = False
                    break

            self.assertFalse(promotion_admitted, "Promotion must be rejected atomically when Anchor 3 drifts")

    # --- Regression Tests for Retry #1 ---

    def test_verify_two_tier_rejects_path_traversal_relative(self):
        """Should reject path traversal attempts with '..' returning error/false (VULN-01)"""
        with tempfile.TemporaryDirectory() as td:
            anchor = TwoTierAnchor()
            anchor.file_path = b"../../etc/passwd"
            anchor.symbol_name = b"root"
            anchor.byte_start = 0
            anchor.byte_len = 10
            anchor.expected_text = b"root:x:0:0"
            anchor.ast_signature_hash = 99999

            # Tier 1 fast match must reject
            fast_match = self.cbm.cbm_fast_offset_match(td.encode("utf-8"), ctypes.byref(anchor))
            self.assertFalse(fast_match, "cbm_fast_offset_match must reject relative path traversal")

            # Tier 2 / Two-Tier verifier must reject
            ok = ctypes.c_bool(True)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )
            self.assertEqual(rc, -1, "cbm_verify_two_tier_anchor must return -1 on path traversal")
            self.assertFalse(ok.value, "ok must be false on path traversal")

    def test_verify_two_tier_rejects_path_traversal_absolute_escape(self):
        """Should reject absolute path attempting to escape or bypass root directory (VULN-01)"""
        with tempfile.TemporaryDirectory() as td:
            anchor = TwoTierAnchor()
            if os.name == "nt":
                anchor.file_path = b"C:/Windows/System32/drivers/etc/hosts"
            else:
                anchor.file_path = b"/etc/passwd"
            anchor.symbol_name = b"escape"
            anchor.byte_start = 0
            anchor.byte_len = 10
            anchor.expected_text = b"localhost"
            anchor.ast_signature_hash = 88888

            # Tier 1 fast match must reject
            fast_match = self.cbm.cbm_fast_offset_match(td.encode("utf-8"), ctypes.byref(anchor))
            self.assertFalse(fast_match, "cbm_fast_offset_match must reject absolute escape path")

            # Tier 2 / Two-Tier verifier must reject
            ok = ctypes.c_bool(True)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )
            self.assertEqual(rc, -1, "cbm_verify_two_tier_anchor must return -1 on absolute escape path")
            self.assertFalse(ok.value, "ok must be false on absolute escape path")

    def test_verify_two_tier_safe_const_anchor_not_mutated(self):
        """Should safely verify anchor without mutating caller's TwoTierAnchor memory (VULN-02)"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "worker.py"
            full_path = Path(td) / file_name
            code = "def do_work():\n    return 42\n"

            p = self.cbm.ts_parser_new()
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_python())
            t = self.cbm.ts_parser_parse_string(p, None, code.encode("utf-8"), len(code))
            m = DeclarationNodeMatch()
            self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"do_work", code.encode("utf-8"), ctypes.byref(m))
            orig_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, code.encode("utf-8"))
            self.cbm.ts_tree_delete(t)
            self.cbm.ts_parser_delete(p)

            # Prepend comment lines to cause relocation
            header = "# License header line 1\n# License header line 2\n"
            full_path.write_bytes((header + code).encode("utf-8"))

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"do_work"
            anchor.byte_start = 0
            anchor.byte_len = len(code.encode("utf-8"))
            anchor.expected_text = code.encode("utf-8")
            anchor.ast_signature_hash = orig_hash

            ok = ctypes.c_bool(False)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )
            self.assertEqual(rc, 0)
            self.assertTrue(ok.value)
            self.assertTrue(reloc.was_relocated)
            # Verify caller anchor was NOT mutated in-place
            self.assertEqual(anchor.byte_start, 0, "Input anchor byte_start must remain unmodified")
            self.assertEqual(anchor.byte_len, len(code.encode("utf-8")), "Input anchor byte_len must remain unmodified")

    def test_verify_two_tier_rejects_zero_ast_hash(self):
        """Should reject malformed anchor with ast_signature_hash == 0 (EDGE-02)"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "test.c"
            full_path = Path(td) / file_name
            code = "int run(void) { return 1; }\n"
            full_path.write_bytes(code.encode("utf-8"))

            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"run"
            anchor.byte_start = 0
            anchor.byte_len = len(code.encode("utf-8"))
            anchor.expected_text = code.encode("utf-8")
            anchor.ast_signature_hash = 0  # Malformed: zero hash

            # Tier 1 fast match must reject even if byte contents match
            fast_match = self.cbm.cbm_fast_offset_match(td.encode("utf-8"), ctypes.byref(anchor))
            self.assertFalse(fast_match, "cbm_fast_offset_match must reject zero ast_signature_hash")

            ok = ctypes.c_bool(True)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )
            self.assertEqual(rc, -1, "cbm_verify_two_tier_anchor must return -1 on zero ast_signature_hash")
            self.assertFalse(ok.value, "ok must be false on zero ast_signature_hash")

    def test_forward_declaration_followed_by_definition_relocates_to_definition(self):
        """Should distinguish prototype forward declaration from definition and relocate to definition (EDGE-01)"""
        with tempfile.TemporaryDirectory() as td:
            file_name = "calc.c"
            full_path = Path(td) / file_name

            proto = "int calculate_total(int count);\n"
            definition = "int calculate_total(int count) {\n    return count * 10;\n}\n"

            # Compute original definition hash
            p = self.cbm.ts_parser_new()
            self.cbm.ts_parser_set_language(p, self.cbm.tree_sitter_c())
            t = self.cbm.ts_parser_parse_string(p, None, definition.encode("utf-8"), len(definition))
            m = DeclarationNodeMatch()
            self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t), b"calculate_total", definition.encode("utf-8"), ctypes.byref(m))
            self.assertEqual(m.node_type, b"function_definition")
            def_hash = self.cbm.cbm_ts_compute_declaration_hash(m.node, definition.encode("utf-8"))
            self.cbm.ts_tree_delete(t)
            self.cbm.ts_parser_delete(p)

            # File has forward declaration, then 10 comment lines, then the full definition
            comment_block = "\n".join([f"// Documentation comment line {i}" for i in range(10)]) + "\n"
            file_content = proto + "\n" + comment_block + "\n" + definition
            full_path.write_bytes(file_content.encode("utf-8"))

            # 1. Test AST declaration search prefers full definition over forward declaration
            p2 = self.cbm.ts_parser_new()
            self.cbm.ts_parser_set_language(p2, self.cbm.tree_sitter_c())
            t2 = self.cbm.ts_parser_parse_string(p2, None, file_content.encode("utf-8"), len(file_content))
            m2 = DeclarationNodeMatch()
            found = self.cbm.cbm_ts_find_declaration_node(self.cbm.ts_tree_root_node(t2), b"calculate_total", file_content.encode("utf-8"), ctypes.byref(m2))
            self.assertTrue(found)
            self.assertEqual(m2.node_type, b"function_definition", "Must prefer full function_definition over prototype")
            self.assertGreater(m2.start_byte, len(proto.encode("utf-8")), "Must point to definition after prototype")
            self.cbm.ts_tree_delete(t2)
            self.cbm.ts_parser_delete(p2)

            # 2. Test two-tier verification correctly relocates to the full function definition
            anchor = TwoTierAnchor()
            anchor.file_path = file_name.encode("utf-8")
            anchor.symbol_name = b"calculate_total"
            anchor.byte_start = 0  # Old pre-shift offset
            anchor.byte_len = len(definition.encode("utf-8"))
            anchor.expected_text = definition.encode("utf-8")
            anchor.ast_signature_hash = def_hash

            ok = ctypes.c_bool(False)
            reloc = TwoTierAnchorRelocation()
            rc = self.cbm.cbm_verify_two_tier_anchor(
                td.encode("utf-8"),
                ctypes.byref(anchor),
                ctypes.byref(ok),
                ctypes.byref(reloc)
            )
            self.assertEqual(rc, 0)
            self.assertTrue(ok.value, "Verification must succeed via Tier 2")
            self.assertTrue(reloc.was_relocated, "was_relocated must be true")
            self.assertTrue(reloc.structural_hash_matched, "structural_hash_matched must be true")

            # Slice should be the definition, containing 'return count * 10'
            matched_slice = file_content.encode("utf-8")[reloc.new_byte_start:reloc.new_byte_start + reloc.new_byte_len]
            self.assertIn(b"return count * 10", matched_slice)
            self.assertNotIn(b"calculate_total(int count);", matched_slice)

    # --- Structural Code Invariants ---

    def test_c_source_contracts_and_struct_invariants(self):
        """Validates all C headers and source code conform to the structural design contracts"""
        checker_h = (REPO_ROOT / "src" / "admission" / "anchor_checker.h").read_text(encoding="utf-8")
        checker_c = (REPO_ROOT / "src" / "admission" / "anchor_checker.c").read_text(encoding="utf-8")
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")

        # Struct TwoTierAnchorRelocation
        self.assertIn("TwoTierAnchorRelocation", checker_h)
        self.assertIn("uint32_t new_byte_start;", checker_h)
        self.assertIn("uint32_t new_byte_len;", checker_h)
        self.assertIn("bool was_relocated;", checker_h)
        self.assertIn("bool structural_hash_matched;", checker_h)

        # Enum CbmSupportedLanguage
        self.assertIn("CbmSupportedLanguage", checker_h)
        self.assertIn("CBM_LANG_C = 1", checker_h)
        self.assertIn("CBM_LANG_TYPESCRIPT = 2", checker_h)
        self.assertIn("CBM_LANG_PYTHON = 3", checker_h)
        self.assertIn("CBM_LANG_UNKNOWN = 0", checker_h)

        # Struct DeclarationNodeMatch and StructuralHashContext
        self.assertIn("DeclarationNodeMatch", checker_h)
        self.assertIn("StructuralHashContext", checker_h)

        # Updated signature
        self.assertIn("TwoTierAnchorRelocation *out_relocation", checker_h)
        self.assertIn("cbm_resolve_language_from_path", checker_h)
        self.assertIn("cbm_ts_find_declaration_node", checker_h)
        self.assertIn("cbm_ts_compute_declaration_hash", checker_h)

        # Tree-sitter wiring in anchor_checker.c
        self.assertIn("tree_sitter_c", checker_c)
        self.assertIn("tree_sitter_typescript", checker_c)
        self.assertIn("tree_sitter_python", checker_c)
        self.assertIn("ts_parser_new", checker_c)
        self.assertIn("ts_parser_parse_string", checker_c)

        # Admission gate call site handles relocation safely without mutating const input
        self.assertIn("TwoTierAnchorRelocation", gate_c)
        self.assertIn("cbm_verify_two_tier_anchor(repo_root, &anchors[i], &ok, &relocs[i])", gate_c)
        self.assertNotIn("(TwoTierAnchor *)&anchors", gate_c)
        self.assertNotIn("(TwoTierAnchor *)anchors", gate_c)


if __name__ == "__main__":
    unittest.main()
