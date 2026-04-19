#!/usr/bin/env python3
"""
MCP server exposing build tools for Blockbit.
Tools: build, build_cmake, build_file, test_world
"""

import json
import os
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(REPO_ROOT, "build")

TOOLS = [
	{
		"name": "build",
		"description": (
			"Build the whole Blockbit project with Ninja. "
			"Returns 'Build succeeded.' on success, or compiler errors on failure. "
			"Auto-configures CMake if build/ doesn't exist yet."
		),
		"inputSchema": {
			"type": "object",
			"properties": {},
			"required": []
		}
	},
	{
		"name": "build_cmake",
		"description": (
			"Regenerate CMake build files. "
			"Use when CMakeLists.txt changes or when source files are added/removed "
			"(the project uses GLOB_RECURSE so cmake must re-run to pick up new files). "
			"Auto-creates build/ if it doesn't exist."
		),
		"inputSchema": {
			"type": "object",
			"properties": {},
			"required": []
		}
	},
	{
		"name": "build_file",
		"description": (
			"Build a single source file for quick error-checking. "
			"Pass the path relative to repo root, e.g. src/Game.cpp. "
			"Auto-configures CMake if build/ doesn't exist yet."
		),
		"inputSchema": {
			"type": "object",
			"properties": {
				"path": {
					"type": "string",
					"description": "Source file path relative to repo root, e.g. src/Game.cpp"
				}
			},
			"required": ["path"]
		}
	},
	{
		"name": "test_world",
		"description": (
			"Build Blockbit and run it loading a specific world. "
			"Pass the world name, e.g. 'Swiat'. "
			"The world must exist in ~/Blockbit/saves/<world-name>."
		),
		"inputSchema": {
			"type": "object",
			"properties": {
				"world": {
					"type": "string",
					"description": "World name to load, e.g. 'Swiat'"
				}
			},
			"required": ["world"]
		}
	}
]


def _format_failure(label, returncode, stdout, stderr):
	"""Return a compact failure string, preferring stderr over stdout."""
	output = stderr.strip() or stdout.strip()
	return f"{label} FAILED (exit {returncode}):\n{output}"


def _ensure_configured():
	"""Run CMake configure if build/ has no CMakeCache. Returns error string or None."""
	cache = os.path.join(BUILD_DIR, "CMakeCache.txt")
	if os.path.isfile(cache):
		return None

	os.makedirs(BUILD_DIR, exist_ok=True)
	result = subprocess.run(
		["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", REPO_ROOT],
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True,
		cwd=BUILD_DIR
	)

	if result.returncode == 0:
		return None

	return _format_failure("CMake configure", result.returncode, result.stdout, result.stderr)


def run_build():
	err = _ensure_configured()
	if err:
		return err

	result = subprocess.run(
		["ninja", "-C", BUILD_DIR],
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True
	)

	if result.returncode == 0:
		return "Build succeeded."

	return _format_failure("Build", result.returncode, result.stdout, result.stderr)


def run_build_cmake():
	os.makedirs(BUILD_DIR, exist_ok=True)
	result = subprocess.run(
		["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", REPO_ROOT],
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True,
		cwd=BUILD_DIR
	)

	if result.returncode == 0:
		return "CMake configure succeeded."

	return _format_failure("CMake configure", result.returncode, result.stdout, result.stderr)


def run_build_file(path):
	err = _ensure_configured()
	if err:
		return err

	path = path.lstrip("./")
	target = f"CMakeFiles/Blockbit.dir/{path}.o"

	result = subprocess.run(
		["ninja", "-C", BUILD_DIR, target],
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True
	)

	if result.returncode == 0:
		return f"Built {path} successfully."

	return _format_failure(f"Build of {path}", result.returncode, result.stdout, result.stderr)


def run_test_world(world):
	build_result = run_build()
	if "FAILED" in build_result:
		return build_result

	binary = os.path.join(BUILD_DIR, "bin", "Blockbit")
	if not os.path.isfile(binary):
		return f"Binary not found: {binary}"

	try:
		result = subprocess.run(
			[binary, "--load", world],
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE,
			text=True,
			cwd=REPO_ROOT,
			timeout=120
		)
	except subprocess.TimeoutExpired:
		return f"World '{world}' ran for 120s (timeout). Likely started OK."

	if result.returncode == 0:
		return f"World '{world}' loaded and exited successfully."

	return _format_failure(f"Run with world '{world}'", result.returncode, result.stdout, result.stderr)


# ---------- MCP protocol ----------

def send(obj):
	sys.stdout.write(json.dumps(obj) + "\n")
	sys.stdout.flush()


def send_response(req_id, result):
	send({"jsonrpc": "2.0", "id": req_id, "result": result})


def send_error(req_id, code, message):
	send({"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}})


def handle_call(req_id, name, args):
	try:
		if name == "build":
			text = run_build()
		elif name == "build_cmake":
			text = run_build_cmake()
		elif name == "build_file":
			path = args.get("path", "").strip()
			if not path:
				send_error(req_id, -32602, "'path' argument is required")
				return
			text = run_build_file(path)
		elif name == "test_world":
			world = args.get("world", "").strip()
			if not world:
				send_error(req_id, -32602, "'world' argument is required")
				return
			text = run_test_world(world)
		else:
			send_error(req_id, -32601, f"Unknown tool: {name}")
			return

		send_response(req_id, {"content": [{"type": "text", "text": text}]})

	except Exception as exc:
		send_error(req_id, -32603, str(exc))


def main():
	for raw in sys.stdin:
		raw = raw.strip()
		if not raw:
			continue

		try:
			msg = json.loads(raw)
		except json.JSONDecodeError:
			continue

		method = msg.get("method", "")
		req_id = msg.get("id")
		params = msg.get("params") or {}

		if method == "initialize":
			send_response(req_id, {
				"protocolVersion": "2024-11-05",
				"capabilities": {"tools": {}},
				"serverInfo": {"name": "blockbit-build", "version": "1.0.0"}
			})
		elif method == "initialized":
			pass
		elif method == "ping":
			send_response(req_id, {})
		elif method == "tools/list":
			send_response(req_id, {"tools": TOOLS})
		elif method == "tools/call":
			handle_call(req_id, params.get("name", ""), params.get("arguments") or {})


if __name__ == "__main__":
	main()
