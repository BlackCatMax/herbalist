// PCGHerbalistWriteResourceSlots.h
//
// «Write Herbalist Resource Slots» (2026-09-19, этап 5б docs/research/
// DESIGN_Living_Vegetation_Research.md §4.1): узел-приёмник графа слотов. Точки
// на входе -- места, где может стоять растение; вид места -- строковый атрибут
// SlotKind («Land», «Shore», «Water»), нет атрибута -- настройка DefaultKind.
// Узел заменяет в ассете слотов карты (Core/World/ResourceSlots.h,
// /Game/Data/ResourceSlots/RS_<карта>) набор своего источника -- актора,
// которому принадлежит исполняющий PCG-компонент. Ассета нет -- создаёт.
// В коммандлете (запекание: -run=WorldPartitionBuilderCommandlet
// -Builder=PCGWorldPartitionBuilder) сохраняет пакет сам, в редакторе
// помечает изменённым.
//
// Только редактор: в игре и в PIE точки проходят насквозь, ассет не трогается
// -- слоты читает менеджер сетки.
#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Core/World/ResourceSlots.h"
#include "PCGHerbalistWriteResourceSlots.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PROJECTHERBALIST_API UPCGHerbalistWriteResourceSlotsSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("WriteHerbalistResourceSlots")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGHerbalistWriteResourceSlots", "NodeTitle", "Write Herbalist Resource Slots"); }
    virtual FText GetNodeTooltipText() const override
    {
        return NSLOCTEXT("PCGHerbalistWriteResourceSlots", "NodeTooltip",
            "Запекает точки в слоты ресурсов карты (RS_<карта>): вид места -- атрибут SlotKind (Land/Shore/Water). Только редактор.");
    }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

    /** Вид места для точек без атрибута SlotKind. */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    EResourceSlotKind DefaultKind = EResourceSlotKind::Land;

    /** Имя строкового атрибута с видом места. */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    FName KindAttribute = TEXT("SlotKind");

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};

class PROJECTHERBALIST_API FPCGHerbalistWriteResourceSlotsElement : public IPCGElement
{
public:
    // Ассет слотов карты: найти или (в редакторе) создать. Тестовый шов.
    static UHerbalistResourceSlots* FindOrCreateSlotsAsset(const FString& MapPackageName, bool bCreateIfMissing);

protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    // Пишет в ассет -- только игровой поток; результат зависит не только от
    // входа (ассет), кэшировать нельзя.
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
