---
tags: [technical, current, journey, artifacts]
gdd: "[[21_Journey_And_Artifacts]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 21. Путешествие и артефакты — техническая сторона

Пара к главе [[21_Journey_And_Artifacts|21. Путешествие — базы, спутники
Аграфены и артефакты]], номера разделов совпадают. Код —
`GridWorldManagerArtifacts.cpp`, `GridWorldManagerArtifactEffects.cpp`,
`GridWorldManagerProphetFeathers.cpp`, `GridWorldManagerBases.cpp`.
Проверка — `docs/verification/pie/07_Zaryana_Journey.md`.

## §21.1 Фрагменты по биомам

`AMemoryFragmentActor`, `DT_MemoryFragments`; фрагменты состояния —
`TrySpawnStateBasedFragment`. Подробно — [[17_Hero_And_Community_Tech]] §17.4,
маршрут — [[23_Journey_Order_Tech]].

## §21.2 Базы и спутники

- Базы — `RegisterBase` (команда `FoundBase X Y`), `GetBases`, сохраняются.
- Зеркальце (`bHasMirror`) и Клубочек (`bHasYarnBall`) — на контроллере,
  выдаются добычей артефакта (или отладкой `GiveZaryanaGifts`). В игре —
  поднести к лицу и взаимодействие; Клубочек спрашивает базу строкой
  выбора ([[07_UX_Tech]] §7.13.4, §7.13.6). Путь клубочком тратит игровое
  время (`YarnBallSecondsPerUnit`).

## §21.3 Артефакты Легендарных

- Определения — `FArtifactDefinition` (`Core/Entities/ArtifactTypes.h`,
  `DT_Artifacts`, `-run=ArtifactsCreate`). Владение — `AcquiredArtifacts`
  (сохраняется), видимый предмет — квитанция в котомке
  (`AddArtifactToInventory`); квитанции не принимают котёл, жертвенник и
  логово (`IsArtifactReceipt`).
- Добыча — `TryAcquireArtifact`: регион очищен
  (`IsLegendaryRegionRestored`), честный путь — реальная чистота дара ≥
  `ArtifactHonestPurityThreshold`, обманный — воспринятая. В игре — дар у
  логова, по одному предмету (`FindArtifactOfLairAt`,
  `OfferForArtifactFromSlots`); команда `OfferForArtifact` — отладка.
- Фонарь — только обманом: приманка Болотного царя
  `TryLureSwampTsarWithPotion` (`LurePotionRadiusMeters`); в игре — зелье из
  руки на клетку его якоря (`LureSwampTsarFromSlot`).
- Эффекты — `UseHornOnCell`, `UseCombOnCell`, `UseYouthApple`,
  `UseInvisibilityCap`, `UseLanternDisclosureOnCell`, заряд Камня-оберега в
  варке (`ResolveBrewModifiers`); в игре — из руки ([[07_UX_Tech]] §7.13.4).
- Калинов мост: плата артефактом после «Сделки» — `TryPayKalinovMostToll`
  (в игре — артефакт из руки на видимого Змея; квитанция уходит тоже).
- Перья вещих птиц — `TryAcquireProphetFeather`, `AcquiredFeathers`;
  применение — `EatGamayunFeather`, `UseAlkonostFeatherOnBiome`,
  `UseSirinFeatherOnCell`, `UseZharPtitsaFeatherOnCell`.

## §21.4 Прогрев

`IsArtifactWarmed`: зелье плюс родной регион (вариант C); Фонарь греется от
общей ясности.

Регрессия: `Herbalist.Artifact.*`, `Herbalist.ArtifactEffects.*`,
`Herbalist.ArtifactInventory.*`, `Herbalist.Bases.*`.
