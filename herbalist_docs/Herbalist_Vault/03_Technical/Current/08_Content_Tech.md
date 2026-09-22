---
tags: [technical, current, content]
gdd: "[[08_Content]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 08. Контент и проявления — техническая сторона

Пара к главе [[08_Content|08. Контент и проявления]], номера разделов
совпадают.

## §8.1 Типы мест

Биомы — регионы `ABiomeRegionVolume` (Blueprint `BP_BiomeVolume`) на карте
([[Level_Assembly]]), параметры — [[10_Biomes_Reference_Tech]]. Места силы —
капища ([[15_Cycles_And_Shrines_Tech]] §15.5), точки интереса (Тотем,
Светлояр, Горюч-камень, Соловей, Калинов мост; `Core/World/POIActors.h`,
посев сам) и курганы (`AKurganActor`) — `docs/verification/pie/05_Places_Entities.md`.

## §8.3 Вода

Вода — регионы `AWaterRegionVolume`; типы воды — `DT_WaterTypes`
(`FWaterTypeRow`), реестр — `UWaterTypeRegistrySubsystem`; сбор воды даёт
предмет с `bIsWater` и `IngredientID` своего типа.

## §8.4 Ресурсы

Карточки, json, таблица — [[05_Systems_Tech]] §5 Система ресурсов. Появление в
мире: кандидат по биому — `UIngredientRegistrySubsystem::GetRandomResourceForBiome`
(сад — `GetRandomResourceForNiche`), места — слоты ресурсов
(`FHerbalistResourceSlot`, `Core/World/ResourceSlots.h`, PCG —
`-run=PcgResourceSlotsSetup`), сбор — `GenerateHarvestResult`.

## §8.5 Условия окружающей среды

Сутки, луна, календарь, погода — [[15_Cycles_And_Shrines_Tech]].

## §8.6 Сущности

[[16_Entity_Manifestation_Tech]].

## §8.12 Капища

[[15_Cycles_And_Shrines_Tech]] §15.5.
