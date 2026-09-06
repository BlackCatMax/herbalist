// PlaytestMapCreateCommandlet.h
//
// "Сделай тестовую карту в движке" (2026-09-06, прямой запрос
// пользователя). Решение пользователя: НЕ трогать `L_TestDev.umap` (он же
// `EditorStartupMap`/`GameDefaultMap`, DefaultEngine.ini — и карта, на
// которой фактически идёт каждый headless-автотест этой сессии; там
// намеренно нет ни одного `ABiomeRegionVolume` — блочный 5x5-фолбэк даёт
// клеткам биом для математики, но реальные биом-регионы изменили бы
// биом конкретных клеток во всех 445 автотестах разом). Вместо этого —
// отдельная, новая карта `/Game/Maps/L_Playtest` с реальными
// биом-регионами (все 8 биомов полосами по Y, тот же порядок север-юг,
// что уже в списке `EBiomeType`), `AGridWorldManager`, `AAlchemyTableActor`
// (домашний якорь — от него уже регистрируется Домовой, BuildHomeStorage
// ищет его как AnchorCell) и `APlayerStart`.
//
// Собрана программно (`UWorld::CreateWorld` + `SpawnActor` + `UPackage::
// SavePackage`), тем же низкоуровневым приёмом, что уже держат все
// *CreateCommandlet этого проекта для DataTable-ассетов — просто для
// `UWorld`, не `UDataTable`. Ландшафта НЕТ (генерация ландшафта кодом —
// отдельная, намного более крупная задача): `GetCellHeight` честно
// возвращает 0 без закэшированных высот, мир получится плоским. Это
// намеренное упрощение теста, не баг — реальный рельеф остаётся
// редакторской/контентной задачей, как и всё остальное визуальное
// наполнение (меши POI-акторов, ландшафт, освещение).
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=PlaytestMapCreate
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PlaytestMapCreateCommandlet.generated.h"

UCLASS()
class UPlaytestMapCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
