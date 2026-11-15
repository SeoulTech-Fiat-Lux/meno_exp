"""manifest.json의 규칙대로 조건별 문맥 묶음을 만든다.

사용: python3 build_context.py <manifest.json> <조건: meno|sfml> <출력 파일> <저장소=로컬경로>...
예:   python3 build_context.py manifest.json sfml sfml.md SFML/SFML=../src/SFML SFML/SFML-Website=../src/site

각 로컬 경로가 git 저장소면 HEAD가 manifest의 커밋과 같은지 확인하고, 다르면 멈춘다.
문서 내용은 고치지 않는다. 파일마다 출처 머리글 한 줄을 달아 이어 붙이고, 끝에 크기와 sha256을 출력한다.
"""
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def head_commit(root: Path) -> str | None:
    try:
        return subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], check=True,
                              capture_output=True, text=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def main() -> None:
    manifest = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    condition, output = sys.argv[2], Path(sys.argv[3])
    roots = dict(arg.split("=", 1) for arg in sys.argv[4:])

    parts: list[str] = []
    for source in manifest[condition]:
        repo, commit = source["repo"], source["commit"]
        root = Path(roots[repo])
        actual = head_commit(root)
        if actual is None:
            print(f"경고: {root}는 git 저장소가 아니라 커밋을 확인하지 못함 ({repo})", file=sys.stderr)
        elif actual != commit:
            sys.exit(f"{repo}: 로컬 HEAD {actual[:7]}가 manifest의 {commit[:7]}와 다름")
        for path in source["files"]:
            text = (root / path).read_text(encoding="utf-8")
            parts.append(f"<<< {repo}@{commit[:7]}:{path} >>>\n{text.rstrip()}\n")

    bundle = "\n".join(parts)
    data = bundle.encode("utf-8")
    # 바이트로 쓴다. write_text는 Windows에서 줄바꿈을 CRLF로 바꿔 쓰므로, 같은 묶음이라도 OS마다 sha256(동결 잠금)이 달라진다.
    output.write_bytes(data)
    print(json.dumps({"condition": condition, "files": len(parts), "bytes": len(data),
                      "chars": len(bundle), "sha256": hashlib.sha256(data).hexdigest()}))


if __name__ == "__main__":
    main()
