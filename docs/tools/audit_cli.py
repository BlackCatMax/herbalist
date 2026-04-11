import subprocess
import os
import sys
from datetime import datetime
from pathlib import Path

# ======================
# CONFIG
# ======================

BASE_DIR = Path(__file__).parent.resolve()

DOCS = list(dict.fromkeys([
    Path("technical.md"),
    Path("design.md"),
    Path("docs") / "build" / "glossary.md"
]))

OUTPUT_DIR = BASE_DIR / "audit_output"
MAX_FILE_SIZE = 20000  # защита от переполнения prompt

# ======================
# UTILS
# ======================

def log(msg):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}")


def run(cmd):
    if isinstance(cmd, str):
        cmd = cmd.split()

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=BASE_DIR
    )

    if result.returncode != 0:
        print("ERROR:", result.stderr)
        sys.exit(1)

    return result.stdout.strip()


def ensure_dirs():
    OUTPUT_DIR.mkdir(exist_ok=True)


def ensure_git_repo():
    try:
        run(["git", "rev-parse", "--is-inside-work-tree"])
    except:
        print("Not a git repository")
        sys.exit(1)


def resolve_path(path: Path) -> Path:
    return (BASE_DIR / path).resolve()


def timestamp():
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def read_file(path: Path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()
    except UnicodeDecodeError:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            return f.read()


def read_file_limited(path: Path):
    content = read_file(path)
    if len(content) > MAX_FILE_SIZE:
        return content[:MAX_FILE_SIZE] + "\n\n...[TRUNCATED]..."
    return content


def write_file_safe(path: Path, content: str):
    if path.exists():
        print(f"File already exists: {path}")
        sys.exit(1)

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


def copy_to_clipboard(text: str):
    try:
        import pyperclip
        pyperclip.copy(text)
        log("✔ Copied to clipboard")
    except:
        log("⚠ pyperclip not installed (pip install pyperclip)")


def open_file(path: Path):
    try:
        if sys.platform == "win32":
            os.startfile(path)
        elif sys.platform == "darwin":
            subprocess.run(["open", str(path)])
        else:
            subprocess.run(["xdg-open", str(path)])
    except:
        pass


def build_docs_block():
    contents = []
    for doc in DOCS:
        full_path = resolve_path(doc)

        if not full_path.exists():
            print(f"Missing file: {full_path}")
            sys.exit(1)

        contents.append(f"\n\n===== FILE: {doc} =====\n\n")
        contents.append(read_file_limited(full_path))

    return ''.join(contents)


# ======================
# STEP 0 — CHECK
# ======================

def check():
    log("Checking environment...")

    ensure_git_repo()

    for doc in DOCS:
        if not resolve_path(doc).exists():
            print(f"Missing: {doc}")
            sys.exit(1)

    log("✔ Environment OK")


# ======================
# STEP 1 — BUILD AUDIT PROMPT
# ======================

def build_audit_prompt(dry=False):
    log("[1] Building audit prompt...")

    ensure_git_repo()

    commit = run(["git", "rev-parse", "HEAD"])
    docs_block = build_docs_block()

    prompt = f"""
Ты — строгий технический аудитор системы.

GIT COMMIT:
{commit}

ЗАДАЧА:
Провести полный аудит согласованности документов.

ПРОВЕРИТЬ:

1. Терминология (строгое совпадение)
2. Пайплайн (порядок этапов)
3. Инварианты
4. Формулы
5. Источники параметров

ФОРМАТ ОТВЕТА (ОБЯЗАТЕЛЕН):

### 🔴 CRITICAL
- [file] проблема
  → как исправить

### 🟡 WARNING
- ...

### 🔵 IMPROVEMENT
- ...

ЗАПРЕЩЕНО:
- переписывать документы
- давать общие советы
- выходить за формат

Если формат нарушен — ответ считается недействительным.

===== DOCUMENTS =====
{docs_block}
"""

    if dry:
        print(prompt)
        return

    path = OUTPUT_DIR / f"audit_prompt_{timestamp()}.md"
    write_file_safe(path, prompt)

    copy_to_clipboard(prompt)
    open_file(path)

    log(f"✔ Saved: {path}")


# ======================
# STEP 2 — BUILD FIX PROMPT
# ======================

def build_fix_prompt(audit_file, dry=False):
    log("[2] Building fix prompt...")

    audit_path = resolve_path(Path(audit_file))

    if not audit_path.exists():
        print(f"Audit file not found: {audit_path}")
        sys.exit(1)

    audit = read_file(audit_path)
    docs_block = build_docs_block()

    prompt = f"""
На основе аудита ниже предложи СТРОГО ТОЧЕЧНЫЕ исправления.

ФОРМАТ (ОБЯЗАТЕЛЕН):

### FILE: path
OLD:
<точный фрагмент>

NEW:
<замена>

ЗАПРЕЩЕНО:
- менять архитектуру
- вводить новые сущности
- переписывать блоки целиком
- изменять формулы без указания в AUDIT
- выходить за формат

Если формат нарушен — ответ считается недействительным.

===== AUDIT =====
{audit}

===== DOCUMENTS =====
{docs_block}
"""

    if dry:
        print(prompt)
        return

    path = OUTPUT_DIR / f"fix_prompt_{timestamp()}.md"
    write_file_safe(path, prompt)

    copy_to_clipboard(prompt)
    open_file(path)

    log(f"✔ Saved: {path}")


# ======================
# STEP 3 — VALIDATE PATCH
# ======================

def validate_patch(patch_file):
    log("[3] Validating patch...")

    patch_path = resolve_path(Path(patch_file))

    if not patch_path.exists():
        print(f"Patch not found: {patch_path}")
        sys.exit(1)

    result = subprocess.run(
        ["git", "apply", "--check", str(patch_path)],
        capture_output=True,
        text=True,
        cwd=BASE_DIR
    )

    if result.returncode != 0:
        print("Patch invalid:")
        print(result.stderr)
        sys.exit(1)

    log("✔ Patch valid")


# ======================
# STEP 4 — APPLY PATCH
# ======================

def apply_patch(patch_file):
    log("[4] Applying patch...")

    validate_patch(patch_file)

    run(["git", "apply", patch_file])

    log("✔ Patch applied")


# ======================
# STEP 5 — DIFF
# ======================

def make_diff():
    log("[5] Creating diff...")

    ensure_git_repo()

    diff = run(["git", "diff"])

    path = OUTPUT_DIR / f"local_changes_{timestamp()}.patch"
    write_file_safe(path, diff)

    log(f"✔ Saved: {path}")


# ======================
# STEP 6 — RE-AUDIT
# ======================

def re_audit():
    log("[6] Re-audit (post-fix)...")
    build_audit_prompt()


# ======================
# CLI
# ======================

def main():
    if len(sys.argv) < 2:
        print("""
Usage:

python audit_cli.py check
python audit_cli.py audit [--dry]
python audit_cli.py fix <audit_file> [--dry]
python audit_cli.py validate <patch>
python audit_cli.py apply <patch>
python audit_cli.py diff
python audit_cli.py reaudit
""")
        sys.exit(0)

    ensure_dirs()

    cmd = sys.argv[1]

    if cmd == "check":
        check()

    elif cmd == "audit":
        dry = "--dry" in sys.argv
        build_audit_prompt(dry)

    elif cmd == "fix":
        if len(sys.argv) < 3:
            print("Provide audit file")
            sys.exit(1)

        dry = "--dry" in sys.argv
        build_fix_prompt(sys.argv[2], dry)

    elif cmd == "validate":
        validate_patch(sys.argv[2])

    elif cmd == "apply":
        apply_patch(sys.argv[2])

    elif cmd == "diff":
        make_diff()

    elif cmd == "reaudit":
        re_audit()

    else:
        print("Unknown command")
        sys.exit(1)

    sys.exit(0)


if __name__ == "__main__":
    main()