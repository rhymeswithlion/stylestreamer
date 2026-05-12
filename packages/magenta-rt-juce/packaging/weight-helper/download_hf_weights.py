#!/usr/bin/env python3
"""Download StyleStreamer C++ MLX weights via the Hugging Face Hub cache."""

from __future__ import annotations

import argparse
import os
import sys
from collections.abc import Sequence
from pathlib import Path

try:
    from huggingface_hub import snapshot_download
except ImportError:  # pragma: no cover - exercised in the packaged app.
    snapshot_download = None  # type: ignore[assignment]


DEFAULT_REPO_ID = "rhymeswithlion/magenta-realtime-mlx-cpp"
os.environ.setdefault("HF_HUB_ENABLE_HF_TRANSFER", "1")


def build_allow_patterns(*, include_mlxfn: bool) -> list[str]:
    patterns = [
        "*.safetensors",
        "depthformer/*.safetensors",
        "*.model",
    ]
    if include_mlxfn:
        patterns.append("mlxfn/*.mlxfn")
    return patterns


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-id", default=DEFAULT_REPO_ID)
    parser.add_argument("--revision", default="main")
    parser.add_argument(
        "--local-dir",
        type=Path,
        default=None,
        help="Optional development mirror directory. Omit to use the canonical HF cache.",
    )
    parser.add_argument(
        "--no-mlxfn",
        action="store_true",
        help="Skip MLXFN graph bundles.",
    )
    parser.add_argument(
        "--print-snapshot-dir",
        action="store_true",
        help="Print SNAPSHOT_DIR=<path> for the JUCE app to parse.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)

    downloader = snapshot_download
    if downloader is None:
        print(
            "huggingface_hub is required. Install it with `python3 -m pip install huggingface_hub[hf_transfer]`.",
            file=sys.stderr,
        )
        return 1

    snapshot_dir = downloader(
        repo_id=args.repo_id,
        repo_type="dataset",
        revision=args.revision,
        allow_patterns=build_allow_patterns(include_mlxfn=not args.no_mlxfn),
        local_dir=str(args.local_dir) if args.local_dir is not None else None,
    )

    if args.print_snapshot_dir:
        print(f"SNAPSHOT_DIR={snapshot_dir}")
    else:
        print(snapshot_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
