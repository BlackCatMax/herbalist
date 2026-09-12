// WorldLayoutSyncBuilder.h
//
// Разметка мира (2026-09-12, DESIGN_World_Layout.md): сверка менеджера сетки
// с ландшафтом и World Partition для карты целиком, без открытия редактора.
//
// Почему билдер World Partition, а не обычный коммандлет: менеджер на карте
// World Partition -- внешний актор, LoadPackage карты его не поднимает
// (WorldStateMapSetup на этом и спотыкался). Билдер инициализирует мир и
// грузит нужные акторы через FWorldPartitionHelpers::ForEachActorWithLoading --
// тот же путь, что у движкового WorldPartitionResaveActorsBuilder.
//
// Запуск:
//   UnrealEditor-Cmd.exe <uproject> -run=WorldPartitionBuilderCommandlet
//       /Game/Maps/L_TestDev -Builder=WorldLayoutSyncBuilder [-ReportOnly]
// -ReportOnly -- посчитать и напечатать разметку, ничего не сохраняя.

#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionBuilder.h"
#include "WorldLayoutSyncBuilder.generated.h"

UCLASS()
class UWorldLayoutSyncBuilder : public UWorldPartitionBuilder
{
    GENERATED_UCLASS_BODY()

public:
    virtual bool RequiresCommandletRendering() const override { return false; }
    virtual ELoadingMode GetLoadingMode() const override { return ELoadingMode::Custom; }

protected:
    virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) override;
};
