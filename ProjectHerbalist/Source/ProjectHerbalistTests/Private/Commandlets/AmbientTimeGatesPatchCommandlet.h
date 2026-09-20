// AmbientTimeGatesPatchCommandlet.h
//
// Даёт временные условия тем Низшим, у кого их не было вовсе (решение
// пользователя 2026-09-20: «у 17 из 33 Низших нет ни суток, ни сезона»).
// Двенадцать карточек получают сутки, сезон или погоду по фольклору своего
// текста; пять остаются круглогодичным фоном биома (Трясинные духи,
// Межевые, Степные духи, Моховые духи, Жердяи) -- их в патче нет.
//
// Зачем: без временного условия карточка подходит всегда, и по правилу
// «редкое вытесняет частое» (2026-09-19) она держит место, пока её не
// сменит кто-то реже. В Тайге и Широколиственном лесу до этой правки не
// было НИ ОДНОГО Низшего с условием по времени -- у обоих биомов не было
// ни суточного, ни годового ритма.
//
// Источник -- herbalist_docs/CSV_tabs/ambient_time_gates.json. Исходник
// генератора (AmbientEntitiesCreateCommandlet) обновлён теми же числами,
// чтобы создание таблицы с нуля давало то же самое.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=AmbientTimeGatesPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AmbientTimeGatesPatchCommandlet.generated.h"

UCLASS()
class UAmbientTimeGatesPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
