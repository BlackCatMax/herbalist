---
tags: [technical, current, ending]
gdd: "[[18_Ending]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 18. Развязка — техническая сторона

Пара к главе [[18_Ending|18. Развязка — три пути у Буяна]], номера разделов
совпадают. Проверка — `docs/verification/pie/07_Zaryana_Journey.md` («Буян»).

## §18.2 Гейтинг

- Условие Буяна — `AGridWorldManager::CheckBuyanCondition`: средний
  `DistanceWithHistory` по клеткам ниже порога и капища восстановлены
  (`BuyanShrineRestorationThreshold`); достигнут — `SetBuyanReached`
  (сохраняется).
- Выбор пути — `TryChooseBuyanPath` (`EBuyanPath`), не переигрывается;
  путь Стража гейтится ясностью и Молвой. Команды: `BecomeBuyanGuardian`,
  `TradePlacesWithZaryana`, `AcceptBuyanReality`; каждая показывает
  гарантированный финальный фрагмент (`BUYAN_GUARDIAN`,
  `BUYAN_TRADE_PLACES`, `BUYAN_ACCEPT_REALITY`, `DT_MemoryFragments`).

## §18.3 Статус

Ветвление работает, тексты трёх сцен — заглушки. Диегетического жеста
выбора пути пока нет — выбор идёт командами; строка выбора
([[07_UX_Tech]] §7.13.6) — готовый механизм для него.
