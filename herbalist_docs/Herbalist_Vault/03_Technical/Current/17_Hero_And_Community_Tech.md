---
tags: [technical, current, community, homestead]
gdd: "[[17_Hero_And_Community]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 17. Герой, наставница и Молва — техническая сторона

Пара к главе [[17_Hero_And_Community|17. Герой, наставница и Молва общины]],
номера разделов совпадают. Проверка —
`docs/verification/pie/06_Community_Homestead.md`,
`docs/verification/pie/07_Zaryana_Journey.md` (фрагменты).

## §17.3 Молва

- `AGridWorldManager::Molva` [−1, 1] (`GridWorldManagerCommunity.cpp`),
  сохраняется; пассивно не меняется.
- Подношение — `OfferToCommunity(Items)`: `MolvaOfferingGain` × сумма
  (Purity − Corruption) по предметам. В игре — камень-жертвенник
  `AOfferingStoneActor` ([[07_UX_Tech]] §7.13.4, [[Level_Assembly]]);
  отладка — `OfferToCommunity "id1,id2"`.
- Торговля — `TryTradeWithCommunity`, курс — `ComputeCommunityTradeValue`
  (Magnitude предмета, редкость карточки, Молва); команда
  `TradeWithCommunity`.
- Разговор и символическое подношение (`SymbolicOfferingRespectGain`) —
  [[16_Entity_Manifestation_Tech]] §16.3. Молва решает, кто пишет заказы —
  [[24_Orders_And_Repute_Tech]].

## §17.4 Фрагменты памяти Заряны

`AMemoryFragmentActor` (`Core/Zaryana/`), определения — `DT_MemoryFragments`
(`GetAllMemoryFragmentDefinitions`, `FMemoryFragmentDefinition`),
13 фрагментов. Текст — `UMemoryRevealWidget` и запись в Травник
(`EJournalEntryType::MemoryFragment`); собранные — `GetCollectedFragmentIDs`
(сохраняются), якорь ясности — `RecomputeClarityAnchorFromFragments`
(глава [[19_Rosa_Signal]]).

## §17.8 Хозяйство

- **Дом** — клетка котла (`AAlchemyTableActor`): Домовой (`RegisterDomovoi`),
  дефолт клетки Заряны. Сон — лавка `ASleepBenchActor` ([[07_UX_Tech]]
  §7.13.7).
- **Переносные контейнеры** — `EquipContainerFromItem` (`GrantsContainerType`
  карточки), в игре — пояс ([[07_UX_Tech]] §7.13.5). Числа порчи —
  [[05_Systems_Tech]] §5 Инвентарь.
- **Домашние хранилища** — `BuildHomeStorage` (погреб, шкаф, кувшин):
  `HomeStorageRespectThreshold` Домового и материал одним стеком; актор —
  `SpawnHomeStorageContainer` класса `HomeStorageContainerClass` (по
  умолчанию `BP_StorageContainer`); по одному каждого типа; сохраняются
  (`CaptureHomeStorages`).
- **Сад** — `RegisterGardenPlot` / команда `SetGardenPlot X Y <ниша>`:
  `EGardenNiche` (Грибница, Погреб, Водоём, Солнечная и Тенистая грядка,
  Грот), цена и порог Молвы — `Core/World/GardenNicheUnlockTypes.h`; ресурсы
  ниши — `GetRandomResourceForNiche`. Посадка — `PlantSeedInCell`
  (`PlantFromSlot`), перегной — `ApplyFertilizerToCell`
  (`FertilizerFertilityBonus`); из руки — [[07_UX_Tech]] §7.13.4.
- **Инструменты** — `EGatheringTool` (`GatheringToolForItem`), серебряный
  оберег — `SetSilverWardActive`; на поясе — [[07_UX_Tech]] §7.13.5.
- **Базы** — `RegisterBase` (команда `FoundBase X Y`), маркеры построек —
  `AHomesteadMarkerActor` (спавнятся при регистрации; после загрузки не
  пересоздаются — известный пробел `ROADMAP.md`).
- Регрессия: `Herbalist.HomeStorage.*`, `Herbalist.GardenNicheUnlock.*`, `Herbalist.GardenPlanting.*`,
  `Herbalist.Community.*`.
