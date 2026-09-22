---
tags: [technical, current, loop]
gdd: "[[04_Game_Loop]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 04. Игровой цикл — техническая сторона

Пара к главе [[04_Game_Loop|04. Игровой цикл]]. Глава описывает цикл
«наблюдение → сбор → варка → применение → мир отвечает»; каждое звено
живёт в своём техдоке:

| Раздел главы | Где устроено |
|---|---|
| Основной цикл | тик мира и пайплайн — [[13_World_Pipeline_Tech]] §13.1 |
| Сбор ресурсов | [[05_Systems_Tech]] §5 Система сбора |
| Алхимия | [[05_Systems_Tech]] §5 Система преобразования (Алхимия); котёл в руке — [[07_UX_Tech]] §7.13.4 |
| Обратная связь с миром | вид предметов, строка ощущения, подсветка — [[07_UX_Tech]] §7.2.3, §7.13 |
| Понимание Заряны | [[19_Rosa_Signal_Tech]], [[17_Hero_And_Community_Tech]] §17.4 |

Проверка всего цикла в PIE — `docs/verification/pie/02_Harvest_Inventory.md`,
`03_Alchemy_Stations.md`; долгий прогон — `docs/verification/ENGINE_VERIFICATION_GUIDE.md`.
