#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Периодическая проверка документации Herbalist.

Только читает -- ничего не исправляет. Что проверяется, как читать отчёт и
что делать с находками -- docs/maintenance/DOCS_AUDIT_CHECKLIST.md.

Запуск из корня репозитория:
    py tools/docs_audit/audit_docs.py                  # все проверки
    py tools/docs_audit/audit_docs.py --only C07,C11   # выбранные
    py tools/docs_audit/audit_docs.py --list           # список проверок
    py tools/docs_audit/audit_docs.py --strict         # падать и на предупреждениях

Отчёт -- build/docs_audit/report.md (build/ в .gitignore).
Код выхода: 0 -- ошибок нет, 1 -- есть ошибки (с --strict -- и предупреждения).
"""

import argparse
import collections
import dataclasses
import datetime
import functools
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    import yaml
except ImportError:  # карточки разберёт упрощённый разбор, сверка воды пропустится
    yaml = None

REPO = Path(__file__).resolve().parents[2]

VAULT = Path("herbalist_docs/Herbalist_Vault")
CSV_TABS = Path("herbalist_docs/CSV_tabs")
SOURCE = Path("ProjectHerbalist/Source")
TESTS_SOURCE = SOURCE / "ProjectHerbalistTests"
COMMANDLETS = TESTS_SOURCE / "Private/Commandlets"
TOOLS_REFERENCE = Path("docs/reference/TOOLS_REFERENCE.md")
DOCS_INDEX = Path("docs/README.md")
VERIFICATION_DOCS = Path("docs/verification")

SEVERITIES = ("error", "warn", "info")

BIOMES = ("Болото", "Лесостепь", "Речная пойма", "Смешанный лес",
          "Широколиственный лес", "Степь", "Тайга", "Тундра")
BIOME_ANYWHERE = "Повсеместно"
# Свободный текст вместо биома у того, что в мире не растёт (Перегной, сад).
NOT_IN_WORLD_PREFIXES = ("Не растёт", "Пещера")
ELEMENTS = ("Вода", "Огонь", "Земля", "Воздух")

# Схема карточек по папкам 04_Compendium. Выведена из самих карточек
# 2026-09-14: эти ключи и разделы есть у каждой карточки категории.
_ITEM_KEYS = ("id", "name", "biome", "type", "d_base", "m_base", "potency", "purity",
              "stability", "resonance", "corruption", "distortion", "element", "tags")
_ITEM_SECTIONS = ("Описание", "Свойства в алхимии", "Где искать", "Применение", "Легенды и поверья")
CARD_SCHEMA = {
    "Растительность": {"keys": _ITEM_KEYS, "sections": _ITEM_SECTIONS, "known_biomes": True},
    "Минералы": {"keys": _ITEM_KEYS + ("resilience",), "sections": _ITEM_SECTIONS, "known_biomes": False},
    "Утварь": {"keys": _ITEM_KEYS + ("resilience",), "sections": _ITEM_SECTIONS, "known_biomes": False},
    "Бестиарий": {
        "keys": ("id", "name", "biome", "type", "level", "behavior", "danger", "d_manifest",
                 "morok_affinity", "tags"),
        "sections": ("Описание", "Параметры проявления", "Где встретить", "Опасность",
                     "Алхимическое значение", "Легенды и поверья"),
        "known_biomes": True,
    },
    "Биомы": {
        "keys": ("id", "name", "type", "potency", "purity", "stability", "resonance", "corruption",
                 "distortion", "body", "mind", "spirit", "nature", "magnitude", "toxicity",
                 "fertility", "moisture", "morok_base", "tags"),
        "sections": ("Общее описание", "Параметры состояния", "Вода", "Обитатели",
                     "Особенности игрового процесса", "Легенды и фольклор"),
        "known_biomes": False,
    },
    "Места_силы": {
        "keys": ("id", "name", "biome", "type", "tags"),
        "sections": ("Описание", "Как читается", "Легенды и поверья"),
        "known_biomes": False,
    },
}
UNIT_FIELDS = ("potency", "purity", "stability", "resonance", "corruption", "distortion", "m_base",
               "morok_affinity", "resilience", "body", "mind", "spirit", "nature", "magnitude",
               "toxicity", "fertility", "moisture", "morok_base")
VECTOR_FIELDS = ("d_base", "d_manifest")
CARD_ENUMS = {
    "level": ("Низший", "Основной", "Легендарный", "Опасная нечисть"),
    "behavior": ("Нейтральный", "Враждебный"),
    "danger": ("Низкая", "Средняя", "Высокая", "Смертельная"),
}
GLOSSARY_STATUSES = ("✅", "🟡 частично", "❌ не реализовано")

# Документы, которые обязаны совпадать с кодом. Аудиты, архив и CHANGELOG --
# летопись: ссылки на удалённое там законны.
CODE_REF_DIRS = (Path("docs/design"), Path("docs/research"), Path("docs/reference"),
                 Path("docs/verification"), Path("docs/maintenance"),
                 VAULT / "01_Glossary", VAULT / "02_GDD", VAULT / "03_Technical/Current",
                 VAULT / "03_Technical/Future")
CODE_REF_FILES = (Path("README.md"), Path("ROADMAP.md"), DOCS_INDEX, Path("docs/DECISIONS_LOG.md"))

# Типы и файлы движка, которые документы законно упоминают. Новая ложная
# находка C11 -- дописать сюда.
ENGINE_NAMES = frozenset({
    "AActor", "ACharacter", "APawn", "APlayerController", "ALandscape", "AWorldSettings",
    "APCGWorldActor", "EAutomationTestFlags", "FBox2D", "FColor", "FHitResult", "FIntPoint",
    "FLinearColor", "FName", "FRandomStream", "FRotator", "FString", "FText", "FTimerDelegate",
    "FTimerHandle", "FTransform", "FVector", "FVector2D", "IConsoleManager", "TArray", "TMap",
    "TSet", "TObjectPtr", "TSubclassOf", "UActorComponent", "UCommandlet", "UDataAsset",
    "UDataTable", "UDeveloperSettings", "UEditorUtilityWidget", "UEngine", "UGameInstance",
    "UGameInstanceSubsystem", "UInputAction", "UInputMappingContext",
    "UMaterialParameterCollection", "UObject", "UPCGComponent", "UPrimaryDataAsset",
    "USaveGame", "UStaticMeshComponent", "UTexture", "UTextureRenderTarget2D", "UUserWidget",
    "UWorld", "UWorldPartition", "UWorldSubsystem",
    "WorldPartition.h", "LandscapeProxy.h", "TimerManager.h",
})

STALE_REPORT_DAYS = 30


@dataclasses.dataclass(frozen=True)
class Finding:
    check: str
    severity: str
    path: str
    line: int
    message: str

    def location(self):
        return f"{self.path}:{self.line}" if self.line else self.path


FRONTMATTER_RE = re.compile(r"\A---[ \t]*\n(.*?)\n---[ \t]*(?:\n|\Z)", re.S)
FENCE_RE = re.compile(r"^(```|~~~).*?^\1[ \t]*$", re.S | re.M)
INLINE_CODE_RE = re.compile(r"`([^`\n]+)`")
WIKILINK_RE = re.compile(r"(!?)\[\[([^\]\|#]*)(#[^\]\|]*)?(\|[^\]]*)?\]\]")
IMAGE_EXT_RE = re.compile(r"\.(png|jpe?g|gif|webp|svg)$", re.I)
HEADING_RE = re.compile(r"^(#{1,6})[ \t]+(.+?)[ \t]*$", re.M)


def line_of(text, index):
    return text.count("\n", 0, index) + 1


def mask_code(text):
    """Код (блоки и `инлайн`) -- пробелами той же длины: номера строк сохраняются."""
    text = FENCE_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)
    return INLINE_CODE_RE.sub(lambda m: " " * len(m.group(0)), text)


def split_frontmatter(text):
    match = FRONTMATTER_RE.match(text)
    return match.group(1) if match else None


def parse_frontmatter(raw):
    """(словарь, ошибка разбора или None)."""
    if yaml is None:
        data = {}
        for line in raw.splitlines():
            key, sep, value = line.partition(":")
            if sep and key and not key.startswith((" ", "-")):
                data[key.strip()] = value.strip() or None
        return data, None
    try:
        data = yaml.safe_load(raw)
    except yaml.YAMLError as exc:
        return {}, str(exc).splitlines()[0]
    return (data if isinstance(data, dict) else {}), None


def flatten_names(value):
    """Имена из поля карточки: скаляр, список, [[Ссылка]] (в YAML -- вложенный список)."""
    if isinstance(value, list):
        return [name for item in value for name in flatten_names(item)]
    if value is None:
        return []
    text = str(value).strip().strip("\"'")
    if text.startswith("[[") and text.endswith("]]"):
        text = text[2:-2]
    return [text.split("|")[0].strip()]


def raw_field(raw, key):
    """Сырой текст поля фронтматтера вместе с блочным списком под ним."""
    match = re.search(r"^%s:(.*(?:\n[ \t]+-.*)*)" % re.escape(key), raw, re.M)
    return match.group(1) if match else ""


class Context:
    def __init__(self, repo):
        self.repo = Path(repo)
        self.vault = self.repo / VAULT
        self._texts = {}

    def rel(self, path):
        return Path(path).relative_to(self.repo).as_posix()

    def read(self, path):
        path = Path(path)
        if path not in self._texts:
            self._texts[path] = path.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
        return self._texts[path]

    def md_under(self, rel_dir):
        base = self.repo / rel_dir
        if not base.exists():
            return []
        return sorted(p for p in base.rglob("*.md")
                      if not any(part in ("build", ".obsidian", ".trash") for part in p.relative_to(base).parts))

    @functools.cached_property
    def vault_md(self):
        return self.md_under(VAULT)

    @functools.cached_property
    def frontmatters(self):
        """путь -> (сырой текст, словарь, ошибка); без фронтматтера -- (None, {}, None)."""
        result = {}
        for path in self.vault_md:
            raw = split_frontmatter(self.read(path))
            data, error = parse_frontmatter(raw) if raw is not None else ({}, None)
            result[path] = (raw, data, error)
        return result

    @functools.cached_property
    def link_targets(self):
        targets = set()
        for path in self.vault_md:
            targets.add(path.relative_to(self.vault).with_suffix("").as_posix().lower())
            targets.add(path.stem.lower())
            for alias in flatten_names(self.frontmatters[path][1].get("aliases")):
                targets.add(alias.lower())
        return targets

    @functools.cached_property
    def vault_file_names(self):
        return {p.name.lower() for p in self.vault.rglob("*") if p.is_file()} if self.vault.exists() else set()

    @functools.cached_property
    def source_texts(self):
        root = self.repo / SOURCE
        if not root.exists():
            return {}
        return {p: self.read(p) for p in root.rglob("*") if p.suffix in (".h", ".cpp", ".cs")}

    @functools.cached_property
    def source_words(self):
        words = set()
        for text in self.source_texts.values():
            words.update(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", text))
        return words

    @functools.cached_property
    def project_file_names(self):
        names = set()
        for rel_dir in (SOURCE, Path("ProjectHerbalist/Config")):
            base = self.repo / rel_dir
            if base.exists():
                names.update(p.name for p in base.rglob("*") if p.is_file())
        return names

    @functools.cached_property
    def quoted_herbalist_names(self):
        """Имена тестов и консольных команд: все строки "Herbalist.*" в коде."""
        names = set()
        for text in self.source_texts.values():
            names.update(re.findall(r'"(Herbalist\.[A-Za-z0-9_.]+)"', text))
        return names

    @functools.cached_property
    def repo_md_names(self):
        skip = (".git", "Intermediate", "Saved", "Binaries", "DerivedDataCache", "node_modules")
        return {p.name for p in self.repo.rglob("*.md") if not any(part in skip for part in p.parts)}

    @functools.cached_property
    def automation_test_count(self):
        count = 0
        base = self.repo / TESTS_SOURCE
        for path, text in self.source_texts.items():
            if base in path.parents:
                count += len(re.findall(r"^\s*IMPLEMENT_SIMPLE_AUTOMATION_TEST\(", text, re.M))
        return count

    def git_date(self, *paths):
        try:
            out = subprocess.run(["git", "-C", str(self.repo), "log", "-1", "--format=%cs", "--", *map(str, paths)],
                                 capture_output=True, text=True, timeout=30)
        except (OSError, subprocess.SubprocessError):
            return None
        value = out.stdout.strip()
        return datetime.date.fromisoformat(value) if out.returncode == 0 and value else None


def compendium_category(ctx, path):
    parts = path.relative_to(ctx.vault).parts
    if len(parts) >= 3 and parts[0] == "04_Compendium" and parts[1] in CARD_SCHEMA:
        return parts[1]
    return None


# ---------------------------------------------------------------------------
# Проверки
# ---------------------------------------------------------------------------

def check_frontmatter(ctx):
    for path, (raw, _data, error) in ctx.frontmatters.items():
        top = path.relative_to(ctx.vault).parts[0]
        if raw is None:
            if top in ("01_Glossary", "02_GDD", "04_Compendium"):
                yield Finding("C01", "error", ctx.rel(path), 1, "нет фронтматтера")
        elif error:
            yield Finding("C01", "error", ctx.rel(path), 1, f"фронтматтер не разбирается: {error}")


def check_card_fields(ctx):
    for path, (raw, data, error) in ctx.frontmatters.items():
        category = compendium_category(ctx, path)
        if not category or raw is None or error:
            continue
        rel = ctx.rel(path)
        schema = CARD_SCHEMA[category]
        missing = [key for key in schema["keys"] if key not in data]
        if missing:
            yield Finding("C02", "error", rel, 1, "нет ключей: " + ", ".join(missing))
        for key in UNIT_FIELDS:
            value = data.get(key)
            if isinstance(value, (int, float)) and not 0.0 <= value <= 1.0:
                yield Finding("C02", "error", rel, 1, f"{key} = {value} вне [0, 1]")
        for key in VECTOR_FIELDS:
            if key not in data:
                continue
            value = data[key]
            if not (isinstance(value, list) and len(value) == 4
                    and all(isinstance(v, (int, float)) and 0.0 <= v <= 1.0 for v in value)):
                yield Finding("C02", "error", rel, 1, f"{key} = {value!r}: нужны 4 числа в [0, 1] (Body, Mind, Spirit, Nature)")
        for key, allowed in CARD_ENUMS.items():
            if key in data and str(data[key]) not in allowed:
                yield Finding("C02", "warn", rel, 1, f"{key} = {data[key]!r}: не из {', '.join(allowed)}")


def normalize_name(text):
    """Для сверки имени карточки с файлом: ё = е, _ = пробел, регистр не важен."""
    text = text.replace("ё", "е").replace("Ё", "Е").replace("ѣ", "е").replace("Ѣ", "Е").replace("_", " ")
    return re.sub(r"\s+", " ", text).strip().lower()


def card_name_matches_file(name, stem):
    """Файл находится по имени карточки: «Журавина (Клюква)» -> Клюква.md,
    «Мухомор (Лесной шут)» -> Мухомор красный.md."""
    name, stem = normalize_name(name), normalize_name(stem)
    primary = name.split("(")[0].strip()
    return stem in name or (primary and primary in stem)


def check_card_ids(ctx):
    by_id = collections.defaultdict(list)
    for path, (raw, data, error) in ctx.frontmatters.items():
        if not compendium_category(ctx, path) or raw is None or error:
            continue
        if data.get("id") is not None:
            by_id[str(data["id"])].append(path)
        # Народное имя с книжным в скобках («Журавина (Клюква)») -- законно:
        # файл обязан лишь находиться по имени карточки.
        name = data.get("name")
        if name is not None and not card_name_matches_file(str(name), path.stem):
            yield Finding("C03", "warn", ctx.rel(path), 1, f"name «{name}» не содержит имени файла «{path.stem}»")
    for card_id, paths in sorted(by_id.items()):
        if len(paths) > 1:
            for path in paths:
                yield Finding("C03", "error", ctx.rel(path), 1, f"id {card_id} повторяется в {len(paths)} карточках")


def check_card_biome_element(ctx):
    for path, (raw, data, error) in ctx.frontmatters.items():
        category = compendium_category(ctx, path)
        if not category or raw is None or error:
            continue
        rel = ctx.rel(path)
        if "biome" in data and CARD_SCHEMA[category]["known_biomes"]:
            names = flatten_names(data["biome"])
            unknown = [n for n in names if n not in BIOMES and n != BIOME_ANYWHERE
                       and not n.startswith(NOT_IN_WORLD_PREFIXES)]
            if unknown:
                yield Finding("C04", "error", rel, 1, "неизвестный биом: " + ", ".join(unknown))
            elif "[[" not in raw_field(raw, "biome") and any(n in BIOMES for n in names):
                yield Finding("C04", "warn", rel, 1, "биом без вики-ссылки [[...]]")
        if "element" in data:
            names = flatten_names(data["element"])
            unknown = [n for n in names if n not in ELEMENTS]
            if unknown:
                yield Finding("C04", "error", rel, 1, "неизвестная стихия: " + ", ".join(unknown))
            elif "[[" not in raw_field(raw, "element"):
                yield Finding("C04", "warn", rel, 1, "стихия без вики-ссылки [[...]] (или вложенный список «- - »)")


def check_card_sections(ctx):
    for path in ctx.vault_md:
        category = compendium_category(ctx, path)
        if not category:
            continue
        text = mask_code(ctx.read(path))
        rel = ctx.rel(path)
        headings = []
        for match in HEADING_RE.finditer(text):
            title = match.group(2)
            if title.count("**") % 2:
                yield Finding("C05", "warn", rel, line_of(text, match.start()), f"непарные ** в заголовке «{title}»")
            headings.append(title.replace("**", "").strip())
        missing = [s for s in CARD_SCHEMA[category]["sections"] if not any(h.startswith(s) for h in headings)]
        if missing:
            yield Finding("C05", "warn", rel, 0, "нет разделов: " + ", ".join(missing))


def check_wikilinks(ctx):
    missing_images = collections.defaultdict(list)
    for path in ctx.vault_md:
        rel_parts = path.relative_to(ctx.vault).parts
        if rel_parts[:2] == ("00_Meta", "Templates"):
            continue
        text = mask_code(ctx.read(path))
        for match in WIKILINK_RE.finditer(text):
            target = match.group(2).strip().rstrip("\\").strip()
            if not target:
                continue
            if match.group(1) or IMAGE_EXT_RE.search(target):
                if Path(target).name.lower() not in ctx.vault_file_names:
                    folder = "/".join(rel_parts[:2]) if rel_parts[0] == "04_Compendium" else rel_parts[0]
                    missing_images[folder].append(Path(target).name)
                continue
            key = target[:-3] if target.lower().endswith(".md") else target
            key = key.lower()
            if key in ctx.link_targets or key.split("/")[-1] in ctx.link_targets:
                continue
            if (ctx.vault / target).exists():
                continue
            yield Finding("C06", "error", ctx.rel(path), line_of(text, match.start()), f"битая ссылка [[{target}]]")
    for folder, names in sorted(missing_images.items()):
        sample = ", ".join(sorted(set(names))[:8])
        yield Finding("C06", "info", (VAULT / folder).as_posix(), 0,
                      f"нет изображений: {len(names)} (например: {sample})")


def check_glossary(ctx):
    base = ctx.vault / "01_Glossary"
    index = base / "_Index.md"
    if not base.exists():
        return
    index_text = ctx.read(index).lower() if index.exists() else ""
    if not index.exists():
        yield Finding("C07", "error", ctx.rel(base), 0, "нет _Index.md")
    for path in sorted(base.glob("*.md")):
        if path.name == "_Index.md":
            continue
        rel = ctx.rel(path)
        if index.exists() and f"[[{path.stem.lower()}" not in index_text:
            yield Finding("C07", "warn", rel, 0, "термин не внесён в 01_Glossary/_Index.md")
        status = ctx.frontmatters.get(path, (None, {}, None))[1].get("status")
        if status is not None and str(status) not in GLOSSARY_STATUSES:
            yield Finding("C07", "warn", rel, 1, f"статус {status!r}: ожидается один из {', '.join(GLOSSARY_STATUSES)}")


def check_gdd(ctx):
    base = ctx.vault / "02_GDD"
    if not base.exists():
        return
    index = base / "_Index.md"
    index_text = ctx.read(index).lower() if index.exists() else ""
    numbers = collections.defaultdict(list)
    for path in sorted(base.glob("*.md")):
        if path.name == "_Index.md":
            continue
        rel = ctx.rel(path)
        match = re.match(r"^(\d{2})_[A-Za-z0-9_]+$", path.stem)
        if not match:
            yield Finding("C08", "warn", rel, 0, "имя главы не по образцу NN_Name.md")
            continue
        number = int(match.group(1))
        numbers[number].append(path)
        if index.exists() and f"[[{path.stem.lower()}" not in index_text:
            yield Finding("C08", "warn", rel, 0, "глава не внесена в 02_GDD/_Index.md")
        status = ctx.frontmatters.get(path, (None, {}, None))[1].get("status")
        if status != "final":
            yield Finding("C08", "info", rel, 1, f"статус главы {status!r} (канон -- final)")
        text = mask_code(ctx.read(path))
        seen = {}
        for heading in HEADING_RE.finditer(text):
            section = re.match(r"(\d+)\.(\d+(?:\.\d+)*)\b", heading.group(2))
            if not section:
                continue
            line = line_of(text, heading.start())
            if int(section.group(1)) != number:
                yield Finding("C08", "warn", rel, line, f"раздел {section.group(0)} в главе {number:02d}")
            full = section.group(0)
            if full in seen:
                yield Finding("C08", "warn", rel, line, f"номер раздела {full} повторяется (первый -- строка {seen[full]})")
            else:
                seen[full] = line
    if numbers:
        for number in range(max(numbers) + 1):
            if number not in numbers:
                yield Finding("C08", "warn", (VAULT / "02_GDD").as_posix(), 0, f"пропущен номер главы {number:02d}")
        for number, paths in numbers.items():
            if len(paths) > 1:
                yield Finding("C08", "warn", (VAULT / "02_GDD").as_posix(), 0,
                              f"номер {number:02d} у нескольких глав: " + ", ".join(p.name for p in paths))


def code_ref_docs(ctx):
    docs = []
    for rel_dir in CODE_REF_DIRS:
        docs.extend(ctx.md_under(rel_dir))
    docs.extend(ctx.repo / rel for rel in CODE_REF_FILES if (ctx.repo / rel).exists())
    return sorted(set(docs))


def check_code_refs(ctx):
    if not ctx.source_texts:
        return
    type_re = re.compile(r"\b([AUFEIT][A-Z][a-z0-9]+(?:[A-Z][A-Za-z0-9]*)+)\b")
    file_re = re.compile(r"\b([A-Za-z0-9_]+\.(?:cpp|h|cs))\b")
    test_re = re.compile(r"\b(Herbalist\.[A-Z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+)")
    for path in code_ref_docs(ctx):
        text = ctx.read(path)
        rel = ctx.rel(path)
        for match in INLINE_CODE_RE.finditer(text):
            code = match.group(1)
            line = line_of(text, match.start())
            for name in sorted(set(type_re.findall(code))):
                if name not in ctx.source_words and name not in ENGINE_NAMES:
                    yield Finding("C09", "warn", rel, line, f"`{name}` нет в коде")
            for name in sorted(set(file_re.findall(code))):
                if name not in ctx.project_file_names and name not in ENGINE_NAMES:
                    yield Finding("C09", "warn", rel, line, f"файла {name} нет в проекте")
            for name in sorted(set(test_re.findall(code))):
                name = name.rstrip(".")
                known = ctx.quoted_herbalist_names
                if name not in known and not any(k.startswith(name + ".") for k in known):
                    yield Finding("C09", "warn", rel, line, f"теста или команды {name} нет в коде")


def check_md_refs(ctx):
    md_re = re.compile(r"([A-Za-z0-9_\-]+\.md)\b")
    for path in code_ref_docs(ctx):
        text = ctx.read(path)
        for match in INLINE_CODE_RE.finditer(text):
            for name in sorted(set(md_re.findall(match.group(1)))):
                if name not in ctx.repo_md_names:
                    yield Finding("C10", "warn", ctx.rel(path), line_of(text, match.start()), f"документа {name} нет в репозитории")


def check_legacy(ctx):
    if ctx.vault.exists():
        for folder in sorted(p for p in ctx.vault.rglob("*") if p.is_dir()):
            if folder.name == "Backups" or folder.name.startswith((".backup", ".translit_backup")):
                yield Finding("C11", "warn", ctx.rel(folder), 0, "резервные копии в репозитории -- история уже в git")
        for path in sorted(ctx.vault.glob("*report*.md")) + sorted((ctx.vault / "00_Meta").glob("*report*.md")):
            yield Finding("C11", "warn", ctx.rel(path), 0, "разовый отчёт скрипта -- устаревает сразу после прогона")
    for path, (raw, data, _error) in ctx.frontmatters.items():
        parts = path.relative_to(ctx.vault).parts
        status = str(data.get("status", ""))
        tags = [str(t) for t in flatten_names(data.get("tags"))]
        if (status in ("superseded", "archived") or "archived" in tags) and "Archive" not in parts:
            yield Finding("C11", "warn", ctx.rel(path), 1, "устаревший документ вне 03_Technical/Archive")
        if parts[:2] == ("03_Technical", "Future") and status == "implemented":
            yield Finding("C11", "warn", ctx.rel(path), 1, "реализованная спецификация лежит в Future")
    drive_re = re.compile(r"(?<![\w/])([A-Z]):[\\/][^\s`'\")\]|]*")
    for path in sorted(set(code_ref_docs(ctx)) | set(ctx.vault_md)):
        text = ctx.read(path)
        for match in drive_re.finditer(text):
            if not Path(f"{match.group(1)}:/").exists():
                yield Finding("C11", "warn", ctx.rel(path), line_of(text, match.start()),
                              f"путь на несуществующем диске: {match.group(0)[:60]}")
    roadmap = ctx.repo / "ROADMAP.md"
    if roadmap.exists():
        text = ctx.read(roadmap)
        for match in re.finditer(r"~~[^~\n]+~~", text):
            yield Finding("C11", "warn", "ROADMAP.md", line_of(text, match.start()),
                          f"закрытый пункт {match.group(0)[:50]} -- по правилу ROADMAP переносится в CHANGELOG и удаляется")


def check_engineering_index(ctx):
    index = ctx.repo / DOCS_INDEX
    if index.exists():
        index_text = ctx.read(index)
        for path in ctx.md_under(Path("docs")):
            if path != index and path.name not in index_text:
                yield Finding("C12", "warn", ctx.rel(path), 0, "документ не внесён в docs/README.md")
    tools_ref = ctx.repo / TOOLS_REFERENCE
    tools_text = ctx.read(tools_ref) if tools_ref.exists() else ""
    commandlets = ctx.repo / COMMANDLETS
    if commandlets.exists():
        for header in sorted(commandlets.glob("*.h")):
            stem = header.stem
            if not stem.endswith(("Commandlet", "Builder")):
                continue
            name = stem[:-len("Commandlet")] if stem.endswith("Commandlet") else stem
            if name not in tools_text:
                yield Finding("C12", "warn", ctx.rel(header), 0, f"коммандлет {name} не описан в {TOOLS_REFERENCE.as_posix()}")
    scripts = sorted((ctx.repo / "tools").rglob("*.py")) if (ctx.repo / "tools").exists() else []
    scripts += sorted(ctx.vault.glob("*.py")) if ctx.vault.exists() else []
    for script in scripts:
        if script.name.startswith("test_") or script.name == "__init__.py":
            continue
        if script.name not in tools_text:
            yield Finding("C12", "warn", ctx.rel(script), 0, f"скрипт не описан в {TOOLS_REFERENCE.as_posix()}")
    verification_text = "\n".join(ctx.read(p) for p in ctx.md_under(VERIFICATION_DOCS))
    exec_re = re.compile(r"UFUNCTION\([^)]*\bExec\b[^)]*\)\s*(?:virtual\s+)?\w[\w<>*&:\s]*?\b(\w+)\s*\(")
    console_re = re.compile(r"FAutoConsoleCommand\w*\s+\w+\(\s*TEXT\(\"([^\"]+)\"\)")
    reported = set()
    for path, text in sorted(ctx.source_texts.items()):
        for regex, kind in ((exec_re, "Exec-команда"), (console_re, "консольная команда")):
            for match in regex.finditer(text):
                name = match.group(1)
                if name in reported or re.search(r"\b%s\b" % re.escape(name), verification_text):
                    continue
                reported.add(name)
                yield Finding("C12", "warn", ctx.rel(path), line_of(text, match.start()),
                              f"{kind} {name} не описана в docs/verification")


def check_verification_facts(ctx):
    count = ctx.automation_test_count
    if not count:
        return
    for path in ctx.md_under(VERIFICATION_DOCS) + ctx.md_under(Path("docs/maintenance")):
        text = ctx.read(path)
        for match in re.finditer(r"Эталон[^\n]*?\*\*(\d+)\s*/\s*\d+\*\*", text):
            if int(match.group(1)) != count:
                yield Finding("C13", "warn", ctx.rel(path), line_of(text, match.start()),
                              f"эталон тестов {match.group(1)}, в коде {count} IMPLEMENT_SIMPLE_AUTOMATION_TEST")
    engine_ini = ctx.repo / "ProjectHerbalist/Config/DefaultEngine.ini"
    if engine_ini.exists():
        match = re.search(r"^GameDefaultMap=/Game/Maps/(\w+)", ctx.read(engine_ini), re.M)
        guides = ctx.md_under(VERIFICATION_DOCS)
        if match and guides and not any(match.group(1) in ctx.read(p) for p in guides):
            yield Finding("C13", "warn", VERIFICATION_DOCS.as_posix(), 0,
                          f"карта по умолчанию {match.group(1)} не упомянута ни в одном гайде проверки")


def check_data_sync(ctx):
    ingredients_json = ctx.repo / CSV_TABS / "ingredients.json"
    if ingredients_json.exists():
        try:
            rows = json.loads(ingredients_json.read_text(encoding="utf-8-sig"))
        except json.JSONDecodeError as exc:
            yield Finding("C14", "error", ctx.rel(ingredients_json), 0, f"json не разбирается: {exc}")
            rows = None
        if isinstance(rows, list):
            json_ids = {str(row.get("Name") or row.get("RowName") or row.get("ID")) for row in rows if isinstance(row, dict)}
            card_ids = {}
            for path, (raw, data, error) in ctx.frontmatters.items():
                if compendium_category(ctx, path) == "Растительность" and data.get("id") is not None:
                    card_ids[str(data["id"])] = path
            for card_id, path in sorted(card_ids.items()):
                if card_id not in json_ids:
                    yield Finding("C14", "warn", ctx.rel(path), 1, f"id {card_id} нет в CSV_tabs/ingredients.json")
    water_script = ctx.repo / "tools/data_extraction/extract_water.py"
    water_json = ctx.repo / CSV_TABS / "water_types.json"
    if water_script.exists() and water_json.exists():
        if yaml is None:
            yield Finding("C14", "info", ctx.rel(water_script), 0, "PyYAML не установлен -- сверка воды пропущена")
            return
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "water_types.json"
            run = subprocess.run([sys.executable, str(water_script), str(out)], capture_output=True, text=True,
                                 encoding="utf-8", errors="replace", timeout=120)
            if run.returncode != 0 or not out.exists():
                yield Finding("C14", "error", ctx.rel(water_script), 0, "скрипт не отработал: " + (run.stderr or run.stdout).strip()[-200:])
            elif json.loads(out.read_text(encoding="utf-8")) != json.loads(water_json.read_text(encoding="utf-8-sig")):
                yield Finding("C14", "warn", ctx.rel(water_json), 0, "расходится с карточками биомов (перегенерировать extract_water.py)")


def check_markers(ctx):
    marker_re = re.compile(r"\b(TODO|TBD|FIXME)\b|\?\?\?")
    for path in ctx.vault_md:
        top = path.relative_to(ctx.vault).parts[0]
        if top not in ("01_Glossary", "02_GDD", "04_Compendium"):
            continue
        text = mask_code(ctx.read(path))
        found = marker_re.findall(text)
        if found:
            yield Finding("C15", "info", ctx.rel(path), 0, f"маркеров незавершённости: {len(found)}")


def check_staleness(ctx):
    current = ctx.vault / "03_Technical/Current"
    source_date = ctx.git_date(ctx.repo / SOURCE)
    if not current.exists() or source_date is None:
        return
    for path in sorted(current.glob("*.md")):
        doc_date = ctx.git_date(path)
        if doc_date and (source_date - doc_date).days > STALE_REPORT_DAYS:
            yield Finding("C16", "info", ctx.rel(path), 0,
                          f"снимок реализации не менялся с {doc_date}, код -- {source_date}: сверить с кодом")


CHECKS = collections.OrderedDict([
    ("C01", ("Фронтматтер есть и разбирается", check_frontmatter)),
    ("C02", ("Карточки: ключи, числа в [0,1], векторы, перечисления", check_card_fields)),
    ("C03", ("Карточки: уникальные id, name = имя файла", check_card_ids)),
    ("C04", ("Карточки: биомы и стихии", check_card_biome_element)),
    ("C05", ("Карточки: обязательные разделы, опечатки в заголовках", check_card_sections)),
    ("C06", ("Хранилище: вики-ссылки и изображения", check_wikilinks)),
    ("C07", ("Глоссарий: индекс и статусы", check_glossary)),
    ("C08", ("GDD: нумерация глав и разделов, индекс, статус", check_gdd)),
    ("C09", ("Ссылки на код: типы, файлы, тесты и команды", check_code_refs)),
    ("C10", ("Ссылки на документы (`*.md`)", check_md_refs)),
    ("C11", ("Легаси: резервные копии, разовые отчёты, устаревшее вне архива, чужие пути, закрытое в ROADMAP", check_legacy)),
    ("C12", ("Инженерные индексы: docs/README, TOOLS_REFERENCE, команды в гайдах проверки", check_engineering_index)),
    ("C13", ("Факты гайдов проверки: число тестов, карта по умолчанию", check_verification_facts)),
    ("C14", ("Данные: карточки против CSV_tabs", check_data_sync)),
    ("C15", ("Маркеры незавершённости в каноне", check_markers)),
    ("C16", ("Снимки реализации 03_Technical/Current против кода", check_staleness)),
])


def run_checks(repo, only=None):
    ctx = Context(repo)
    findings = []
    for check_id, (_title, func) in CHECKS.items():
        if only and check_id not in only:
            continue
        findings.extend(func(ctx))
    return findings


def write_report(findings, repo, path, only):
    counts = collections.Counter((f.check, f.severity) for f in findings)
    head = subprocess.run(["git", "-C", str(repo), "rev-parse", "--short", "HEAD"], capture_output=True, text=True)
    lines = [
        "# Проверка документации",
        "",
        f"{datetime.datetime.now():%Y-%m-%d %H:%M}, коммит `{head.stdout.strip() or '?'}`. "
        "Как читать и что делать -- `docs/maintenance/DOCS_AUDIT_CHECKLIST.md`.",
        "",
        "| Проверка | Ошибки | Предупр. | Инфо |",
        "|---|---:|---:|---:|",
    ]
    for check_id, (title, _func) in CHECKS.items():
        if only and check_id not in only:
            continue
        lines.append(f"| {check_id} {title} | {counts[(check_id, 'error')]} | {counts[(check_id, 'warn')]} | {counts[(check_id, 'info')]} |")
    by_check = collections.defaultdict(list)
    for finding in findings:
        by_check[finding.check].append(finding)
    for check_id, items in by_check.items():
        lines += ["", f"## {check_id} {CHECKS[check_id][0]}", ""]
        for finding in sorted(items, key=lambda f: (SEVERITIES.index(f.severity), f.path, f.line)):
            lines.append(f"- **{finding.severity}** `{finding.location()}` -- {finding.message}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description="Проверка документации Herbalist (только чтение).")
    parser.add_argument("--repo", type=Path, default=REPO, help="корень репозитория")
    parser.add_argument("--only", help="проверки через запятую, например C06,C09")
    parser.add_argument("--report", type=Path, help="куда писать отчёт (по умолчанию build/docs_audit/report.md)")
    parser.add_argument("--strict", action="store_true", help="код выхода 1 и на предупреждениях")
    parser.add_argument("--list", action="store_true", help="список проверок")
    args = parser.parse_args(argv)
    if args.list:
        for check_id, (title, _func) in CHECKS.items():
            print(f"{check_id}  {title}")
        return 0
    only = {c.strip().upper() for c in args.only.split(",")} if args.only else None
    unknown = (only or set()) - set(CHECKS)
    if unknown:
        parser.error("неизвестные проверки: " + ", ".join(sorted(unknown)))
    findings = run_checks(args.repo, only)
    report = args.report or args.repo / "build/docs_audit/report.md"
    write_report(findings, args.repo, report, only)
    totals = collections.Counter(f.severity for f in findings)
    by_check = collections.Counter((f.check, f.severity) for f in findings)
    for check_id, (title, _func) in CHECKS.items():
        if only and check_id not in only:
            continue
        print(f"{check_id} {title}: ошибок {by_check[(check_id, 'error')]}, "
              f"предупреждений {by_check[(check_id, 'warn')]}, инфо {by_check[(check_id, 'info')]}")
    print(f"\nИтого: ошибок {totals['error']}, предупреждений {totals['warn']}, инфо {totals['info']}. Отчёт: {report}")
    failed = totals["error"] > 0 or (args.strict and totals["warn"] > 0)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
