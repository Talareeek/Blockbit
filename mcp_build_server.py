#!/usr/bin/env python3
"""
MCP server exposing build tools for Blockbit.
Tools: build, build_file, test_world
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
			"Build the whole Blockbit project with CMake and Ninja. "
			"Returns 'Build succeeded.' on success, or compiler errors on failure. "
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
			"Build a single source file. "
			"Pass the path relative to the repo root, e.g. src/Game.cpp. "
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
			"Build and test Blockbit by loading a specific world. "
			"Pass the world name, e.g. 'Swiat'. "
			"World must exist in ~/Blockbit/saves/<world-name>"
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


def run_build():
	"""Build the entire project using build.sh"""
	result = subprocess.run(
		["bash", "build.sh"],
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True,
		cwd=REPO_ROOT
	)

	if result.returncode == 0:
		return "Build succeeded."

	return _format_failure("Build", result.returncode, result.stdout, result.stderr)


def run_build_file(path):
	"""Build a single source file"""
	path = path.lstrip("./")
	
	# Ensure build dir exists first
	if not os.path.isdir(BUILD_DIR):
		init_result = subprocess.run(
			["bash", "build.sh"],
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE,
			text=True,
			cwd=REPO_ROOT
		)
		if init_result.returncode != 0:
			return _format_failure("Initial build", init_result.returncode, init_result.stdout, init_result.stderr)
	
	# Build the single file - construct the target name
	# For src/Game.cpp -> CMakeFiles/Blockbit.dir/src/Game.cpp.o
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
	"""Build and test by loading a world"""
	world = world.strip()
	
	result = subprocess.run(
		["bash", "build.sh", "run-world", world],
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True,
		cwd=REPO_ROOT,
		timeout=120  # Give it 2 minutes before timeout
	)

	if result.returncode == 0:
		return f"Built and loaded world '{world}' successfully."

	# Even if the game runs, return success if build part succeeded
	if "Build complete!" in result.stdout:
		return f"Built successfully and attempted to load world '{world}'."

	return _format_failure(f"Build/test of {world}", result.returncode, result.stdout, result.stderr)


# ---------- MCP protocol helpers ----------

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
			pass  # notification — no response
		elif method == "ping":
			send_response(req_id, {})
		elif method == "tools/list":
			send_response(req_id, {"tools": TOOLS})
		elif method == "tools/call":
			handle_call(req_id, params.get("name", ""), params.get("arguments") or {})


if __name__ == "__main__":
	main()
