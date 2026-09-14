# herbalist_docs

Каноническая документация игры и её машинное отражение.

- `Herbalist_Vault/` — хранилище Obsidian: `01_Glossary` (термины), `02_GDD`
  (канон механик), `03_Technical` (`Current` — снимки текущей реализации,
  `Archive` — история), `04_Compendium` (карточки: растения, минералы, утварь,
  бестиарий, биомы, места силы), `_MOC.md` — карта документации.
- `CSV_tabs/` — json и csv для DataTable. Порядок правды: документация →
  json → ассет.

Склейки глав и карточек в `Herbalist_Vault/build/` (в git не попадают):
`py herbalist_docs/Herbalist_Vault/build_docs.py`.

Проверка документации: `py tools/docs_audit/audit_docs.py`, как читать —
`docs/maintenance/DOCS_AUDIT_CHECKLIST.md`.
