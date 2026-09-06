// DomovoiMilkOfferingPatchCommandlet.h
//
// Точечная правка живого DT_Dialogue (2026-09-06, решение пользователя):
// ветка "Оставить у печи блюдце молока" (Домовой, узел "Home", первая
// ветка) до этой правки не имела игрового эффекта (FDialogueBranch::
// bIsSymbolicOffering не существовало на момент постройки таблицы
// DialogueCreateCommandlet'ом). DialogueCreateCommandlet сам идемпотентен
// только в сторону "таблица уже есть — ничего не делаю", он не умеет
// патчить уже существующий ряд — та же ситуация, что уже решалась
// отдельными Patch-коммандлетами для DT_IngredientClass/DT_AmbientEntities.
//
// Точечная правка через Table->FindRow -- тот же приём, что и у остальных
// Patch-коммандлетов: находит ряд "Домовой", узел "Home" внутри него,
// первую ветку (по ActionText, не по индексу — устойчивее к будущим
// правкам порядка веток) и выставляет bIsSymbolicOffering=true. Один
// хардкоженный ряд, не JSON-патч — правка слишком мала и одноразова для
// отдельного файла данных.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=DomovoiMilkOfferingPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DomovoiMilkOfferingPatchCommandlet.generated.h"

UCLASS()
class UDomovoiMilkOfferingPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
