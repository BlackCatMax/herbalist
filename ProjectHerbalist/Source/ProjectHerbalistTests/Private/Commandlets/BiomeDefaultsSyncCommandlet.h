// BiomeDefaultsSyncCommandlet.h
//
// Приводит /Game/Data/DT_BiomeDefaults к КОМПЕНДИУМУ (2026-09-07, решение
// пользователя: "верить надо не json, а документации").
//
// Зачем понадобилось. Сверка трёх источников 2026-09-07 показала: экспорт
// `herbalist_docs/CSV_tabs/DT_BiomeDefaults.json` совпадает с карточками
// компендиума ТОЧНО, во всех 32 значениях (8 биомов x 4 мета-оси), а
// расходится с ними САМ АССЕТ — в 15 значениях. То есть устарел не
// документный экспорт, как предполагалось, а данные, по которым идёт игра.
// Найдено только теперь, потому что до 2026-09-07 автотесты молча гонялись
// на НУЛЕВЫХ дефолтах биомов и настоящих чисел не видели вовсе.
//
// Источник правды — карточки `04_Compendium/Биомы/*.md` (frontmatter).
// Числа перенесены оттуда построчно и ничем не «уточнялись».
//
// Что НЕ трогается, намеренно: `EntityActivityBase`, `DefaultWaterState` и
// `StressRecoveryMultiplier`. Компендиум их не задаёт вовсе (у биомных
// карточек поля `water: []` пустые), а `StressRecoveryMultiplier` по
// собственному комментарию в `BiomeRow.h` ВЫВОДИТСЯ скриптом
// `extract_biomes.py` из Fertility/Distortion и характера воды — то есть у
// него свой, производный источник, и затирать его прямой записью нельзя.
// Перезаписывать поле, для которого у документации нет значения, значило бы
// молча выдумать данные.
//
// Идемпотентен: значение, уже совпадающее с документацией, пропускается;
// в лог печатается полный список изменений «было -> стало».
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=BiomeDefaultsSync
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BiomeDefaultsSyncCommandlet.generated.h"

UCLASS()
class UBiomeDefaultsSyncCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
