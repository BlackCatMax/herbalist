#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Тесты проверки документации на искусственном мини-репозитории.

Запуск из корня репозитория: py -m unittest tools/docs_audit/test_audit_docs.py
"""

import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_docs  # noqa: E402

VAULT = "herbalist_docs/Herbalist_Vault"

PLANT_CARD = """\
---
id: bol_01
name: Багульник
biome: [[Болото]]
type: Кустарник
d_base: [0.2, 0.6, 0.8, 0.7]
m_base: 0.5
potency: 0.7
purity: 0.4
stability: 0.3
resonance: 0.8
corruption: 0.6
distortion: 0.7
element: [[Вода]]
tags: [болото]
---

## Описание
## Свойства в алхимии
## Где искать
## Применение
## Легенды и поверья
"""


BIOME_CARD = """\
---
id: boloto
name: Болото
type: чёрная, стоячая
potency: 0.68
purity: 0.35
stability: 0.35
resonance: 0.75
corruption: 0.70
distortion: 0.70
body: 0.40
mind: 0.40
spirit: 0.60
nature: 0.60
magnitude: 0.75
toxicity: 0.70
fertility: 0.60
moisture: 0.90
morok_base: 0.70
tags: [биом]
---

## Общее описание
## Параметры состояния ([[Purity]])
## Вода
## Обитатели
## Особенности игрового процесса
## Легенды и фольклор
"""


class AuditFixture(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.repo = Path(self._tmp.name)
        self.write(f"{VAULT}/01_Glossary/_Index.md", "---\ntags: [glossary]\n---\n- [[Purity]] ✅\n- [[Вода]] ✅\n")
        self.write(f"{VAULT}/01_Glossary/Purity.md", "---\ntags: [glossary]\nstatus: ✅\naliases: [Чистота]\n---\n# Purity\n")
        self.write(f"{VAULT}/01_Glossary/Вода.md", "---\ntags: [glossary]\nstatus: ✅\n---\n# Вода\n")
        self.write(f"{VAULT}/04_Compendium/Биомы/Болото.md", BIOME_CARD)
        self.write(f"{VAULT}/02_GDD/_Index.md", "---\ntags: [gdd]\n---\n- [[00_Core_Lock]]\n- [[01_Introduction]]\n")
        self.write(f"{VAULT}/02_GDD/00_Core_Lock.md", "---\ntags: [gdd]\nstatus: final\n---\n# 0. Ядро\n## 0.1 Суть\nСм. [[Purity]] и [[Чистота]].\n")
        self.write(f"{VAULT}/02_GDD/01_Introduction.md", "---\ntags: [gdd]\nstatus: final\n---\n# 1. Введение\n## 1.1 Мир\n")
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Багульник.md", PLANT_CARD)
        self.write("ProjectHerbalist/Source/ProjectHerbalist/Core/GridWorldManager.h",
                   "class AGridWorldManager {\n    UFUNCTION(Exec)\n    void ReportGridCorruption();\n};\n")
        self.write("ProjectHerbalist/Source/ProjectHerbalistTests/Private/Tests/GridTest.cpp",
                   'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGridTest, "Herbalist.Grid.Works", 0)\n')
        self.write("ProjectHerbalist/Source/ProjectHerbalistTests/Private/Commandlets/BiomeDefaultsSyncCommandlet.h", "// sync\n")
        self.write("docs/README.md", "# Документы\n`ENGINE_VERIFICATION_GUIDE.md`, `TOOLS_REFERENCE.md`\n")
        self.write("docs/reference/TOOLS_REFERENCE.md", "| `-run=BiomeDefaultsSync` | ... |\n")
        self.write("docs/verification/ENGINE_VERIFICATION_GUIDE.md",
                   "Эталон: **1 / 0**. Команда `ReportGridCorruption`, тест `Herbalist.Grid.Works`, "
                   "класс `AGridWorldManager`, файл `GridWorldManager.h`.\n")

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, rel, text):
        path = self.repo / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(text), encoding="utf-8")

    def findings(self, *checks):
        return audit_docs.run_checks(self.repo, set(checks) or None)

    def messages(self, *checks, severity=None):
        return [f.message for f in self.findings(*checks) if severity is None or f.severity == severity]

    def assertAnyMessage(self, needle, messages):
        self.assertTrue(any(needle in m for m in messages), f"нет «{needle}» среди {messages}")


class CleanFixtureTest(AuditFixture):
    def test_clean_fixture_has_no_errors_or_warnings(self):
        problems = [f for f in self.findings() if f.severity in ("error", "warn")]
        self.assertEqual(problems, [], "чистый набор даёт находки -- остальные тесты ловили бы шум")

    def test_incomplete_biome_card_is_caught(self):
        self.write(f"{VAULT}/04_Compendium/Биомы/Болото.md", "---\nid: boloto\nname: Болото\n---\n")
        self.assertAnyMessage("нет ключей: type", self.messages("C02", severity="error"))


class LinkTest(AuditFixture):
    def test_broken_link_reported_alias_and_code_span_are_not(self):
        self.write(f"{VAULT}/02_GDD/01_Introduction.md",
                   "---\nstatus: final\n---\n# 1. Введение\n[[Нет такого]] `[[В коде]]` [[Чистота|чисто]]\n")
        broken = [f for f in self.findings("C06") if f.severity == "error"]
        self.assertEqual([f.message for f in broken], ["битая ссылка [[Нет такого]]"])
        self.assertEqual(broken[0].line, 5)

    def test_missing_image_is_aggregated_info(self):
        self.write(f"{VAULT}/04_Compendium/Бестиарий/Болото/Болотник.md", "---\nid: x\n---\n![[bolotnik.png]]\n")
        infos = self.messages("C06", severity="info")
        self.assertAnyMessage("нет изображений: 1", infos)


class CardTest(AuditFixture):
    def test_missing_key_out_of_range_and_bad_vector(self):
        card = PLANT_CARD.replace("purity: 0.4\n", "").replace("potency: 0.7", "potency: 1.7")
        card = card.replace("[0.2, 0.6, 0.8, 0.7]", "[0.2, 0.6]")
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Багульник.md", card)
        messages = self.messages("C02")
        self.assertAnyMessage("нет ключей: purity", messages)
        self.assertAnyMessage("potency = 1.7 вне [0, 1]", messages)
        self.assertAnyMessage("d_base", messages)

    def test_duplicate_id_and_name_mismatch(self):
        self.write(f"{VAULT}/04_Compendium/Растительность/Тайга/Брусника.md", PLANT_CARD)
        messages = self.messages("C03")
        self.assertAnyMessage("id bol_01 повторяется в 2 карточках", messages)
        self.assertAnyMessage("name «Багульник» не содержит имени файла «Брусника»", messages)

    def test_folk_name_with_book_name_and_yo_are_accepted(self):
        card = PLANT_CARD.replace("name: Багульник", "name: Журавина (Клюква)")
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Клюква.md", card.replace("bol_01", "bol_02"))
        card = PLANT_CARD.replace("name: Багульник", "name: Берёза_пушистая").replace("bol_01", "bol_03")
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Береза пушистая.md", card)
        card = PLANT_CARD.replace("name: Багульник", "name: Мухомор (Лесной шут)").replace("bol_01", "bol_04")
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Мухомор красный.md", card)
        self.assertEqual(self.messages("C03"), [])

    def test_not_growing_item_may_describe_biome_in_words(self):
        card = PLANT_CARD.replace("biome: [[Болото]]", 'biome: "Не растёт нигде — из сгнившего растения"')
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Багульник.md", card)
        self.assertEqual(self.messages("C04"), [])

    def test_unknown_biome_and_unlinked_element(self):
        card = PLANT_CARD.replace("biome: [[Болото]]", "biome: [[Марс]]").replace("element: [[Вода]]", "element: Вода")
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Багульник.md", card)
        self.assertAnyMessage("неизвестный биом: Марс", self.messages("C04", severity="error"))
        self.assertAnyMessage("стихия без вики-ссылки", self.messages("C04", severity="warn"))

    def test_heading_typo_and_missing_section(self):
        card = PLANT_CARD.replace("## Где искать\n", "## Где искать**\n").replace("## Применение\n", "")
        self.write(f"{VAULT}/04_Compendium/Растительность/Болото/Багульник.md", card)
        messages = self.messages("C05")
        self.assertAnyMessage("непарные ** в заголовке", messages)
        self.assertAnyMessage("нет разделов: Применение", messages)


class GlossaryAndGddTest(AuditFixture):
    def test_glossary_term_missing_from_index_and_bad_status(self):
        self.write(f"{VAULT}/01_Glossary/Stability.md", "---\ntags: [glossary]\nstatus: draft\n---\n# Stability\n")
        messages = self.messages("C07")
        self.assertAnyMessage("термин не внесён", messages)
        self.assertAnyMessage("статус 'draft'", messages)

    def test_gdd_gap_duplicate_section_and_foreign_number(self):
        self.write(f"{VAULT}/02_GDD/03_Narrative.md",
                   "---\nstatus: draft\n---\n# 3. Сюжет\n## 3.1 Начало\n## 3.1 Снова\n## 4.2 Чужой\n")
        messages = self.messages("C08")
        self.assertAnyMessage("пропущен номер главы 02", messages)
        self.assertAnyMessage("номер раздела 3.1 повторяется", messages)
        self.assertAnyMessage("раздел 4.2 в главе 03", messages)
        self.assertAnyMessage("глава не внесена", messages)
        self.assertAnyMessage("статус главы 'draft'", self.messages("C08", severity="info"))


class CodeRefTest(AuditFixture):
    def test_stale_type_file_test_and_document(self):
        self.write("docs/design/DESIGN_X.md",
                   "`AGridWorldManager` `UHarvestService` `HarvestService.cpp` `FVector` "
                   "`Herbalist.Grid` `Herbalist.Grid.Removed` `DESIGN_Gone.md` `README.md`\n")
        self.write("README.md", "корень\n")
        messages = self.messages("C09")
        self.assertEqual(sorted(messages), sorted([
            "`UHarvestService` нет в коде",
            "файла HarvestService.cpp нет в проекте",
            "теста или команды Herbalist.Grid.Removed нет в коде",
        ]))
        self.assertEqual(self.messages("C10"), ["документа DESIGN_Gone.md нет в репозитории"])


class LegacyAndIndexTest(AuditFixture):
    def test_backups_report_superseded_foreign_drive_and_roadmap(self):
        self.write(f"{VAULT}/00_Meta/Backups/20260422/05_Systems.md", "old\n")
        self.write(f"{VAULT}/docs_check_report.md", "report\n")
        self.write(f"{VAULT}/03_Technical/Future/old_spec.md", "---\nstatus: superseded\n---\n")
        self.write(f"{VAULT}/03_Technical/Future/graph.md", "---\nstatus: implemented\n---\n")
        self.write(f"{VAULT}/02_GDD/01_Introduction.md", "---\nstatus: final\n---\n# 1. Введение\nПапка Q:\\old\\vault\n")
        self.write("ROADMAP.md", "- ~~Сделано давно~~ Исправлено\n")
        messages = self.messages("C11")
        self.assertAnyMessage("резервные копии", messages)
        self.assertAnyMessage("разовый отчёт", messages)
        self.assertAnyMessage("устаревший документ вне", messages)
        self.assertAnyMessage("реализованная спецификация лежит в Future", messages)
        self.assertAnyMessage("путь на несуществующем диске: Q:\\old\\vault", messages)
        self.assertAnyMessage("закрытый пункт ~~Сделано давно~~", messages)

    def test_unlisted_doc_commandlet_script_and_exec(self):
        self.write("docs/design/DESIGN_New.md", "# Новое\n")
        self.write("ProjectHerbalist/Source/ProjectHerbalistTests/Private/Commandlets/MapSetupCommandlet.h", "\n")
        self.write("tools/data_extraction/extract_new.py", "\n")
        self.write("ProjectHerbalist/Source/ProjectHerbalist/Player/Controller.h",
                   "UFUNCTION(Exec)\nvoid HarvestHere();\nstatic FAutoConsoleCommandWithWorld Cmd(\n    TEXT(\"Herbalist.Graph.Print\"),\n")
        messages = self.messages("C12")
        self.assertAnyMessage("документ не внесён в docs/README.md", messages)
        self.assertAnyMessage("коммандлет MapSetup не описан", messages)
        self.assertAnyMessage("скрипт не описан", messages)
        self.assertAnyMessage("Exec-команда HarvestHere не описана", messages)
        self.assertAnyMessage("консольная команда Herbalist.Graph.Print не описана", messages)
        self.assertFalse(any("ReportGridCorruption" in m for m in messages), "описанная команда не должна ловиться")

    def test_test_count_reference_mismatch(self):
        self.assertEqual(self.messages("C13"), [], "sanity: эталон 1 совпадает с одним тестом")
        self.write("ProjectHerbalist/Source/ProjectHerbalistTests/Private/Tests/More.cpp",
                   'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMore, "Herbalist.Grid.More", 0)\n'
                   '// IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOff, "Herbalist.Grid.Off", 0)\n')
        self.assertEqual(self.messages("C13"), ["эталон тестов 1, в коде 2 IMPLEMENT_SIMPLE_AUTOMATION_TEST"])


if __name__ == "__main__":
    unittest.main()
