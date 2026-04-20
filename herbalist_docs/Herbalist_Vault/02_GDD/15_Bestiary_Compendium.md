---
tags: [gdd, bestiary, reference]
---
# Бестиарий (Сущности)

Данный раздел содержит полный перечень существ, встречающихся в мире **Herbalist**, с их описаниями, параметрами и особенностями.

## Оглавление по биомам

### Тундра
- [[Сирин]]
- [[Волот]]
- [[Хозяин Севера]]
- ...

### Тайга
- ...

## Dataview: все существа

```dataview
TABLE without ID
  file.link as Название,
  biome as Биом,
  level as Уровень,
  danger as Опасность
FROM "docs/bestiary"
SORT biome ASC, level DESC