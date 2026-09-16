// Core/World/Sky/LeafFallSubsystem.h
//
// Листопад у игрока (2026-09-17, этап 4 docs/research/
// DESIGN_Living_Vegetation_Research.md §3.2): система Niagara на пешке игрока
// в лиственных биомах, темп -- из сезона (AGridWorldManager::GetLeafFall01) и
// ветра симуляции. Сама система частиц -- ассет художника
// (UHerbalistSettings::LeafFallSystem); пусто -- подсистема молчит.
//
// Пользовательские параметры системы: LeafFallRate (0..1) и WindIntensity
// (0..1). Подстилка на земле и редеющая листва -- материалы (MF_SeasonWeights:
// LeafLitter01, MF_LeafDrop), не эта подсистема.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "LeafFallSubsystem.generated.h"

class AGridWorldManager;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class PROJECTHERBALIST_API ULeafFallSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // Editor -- ради автотестов; тикает подсистема только в игровых мирах.
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;

    // Темп листопада в точке: 0 вне лиственных биомов и за сеткой, иначе
    // LeafFall01 x ((1 - WindShare) + WindShare x ветер).
    float ComputeLeafFallRate(const AGridWorldManager& Manager, const FVector& Location) const;

    // Чистая формула темпа (тесты).
    static float LeafFallRate(float LeafFall01, float Wind01, bool bDeciduous, float WindShare);

    // Применяет темп к системе на пешке: создаёт компонент при первом
    // ненулевом темпе. Нулевой темп -- сначала просто LeafFallRate = 0
    // (компонент активен: короткая пауза вроде клетки с водой не должна
    // обрывать листопад), деактивация -- после IdleDeactivateSeconds
    // подряд. Деактивированная система Niagara считается активной, пока
    // летят старые частицы, поэтому своё состояние -- bRequestedActive, не
    // IsActive(). Тестовый шов: система и владелец задаются явно.
    void ApplyRate(AActor* Owner, UNiagaraSystem* System, float Rate, float Wind01, float DeltaSeconds);

    bool IsLeafFallRequestedActive() const { return bRequestedActive; }

    static constexpr float IdleDeactivateSeconds = 10.0f;

    UNiagaraComponent* GetLeafFallComponent() const { return LeafFallComponent.Get(); }

    static const FName LeafFallRateParameter;
    static const FName WindIntensityParameter;

private:
    TWeakObjectPtr<UNiagaraComponent> LeafFallComponent;
    TWeakObjectPtr<AGridWorldManager> CachedManager;
    TWeakObjectPtr<UNiagaraSystem> CachedSystem;
    bool bSystemResolved = false;
    bool bRequestedActive = false;
    float ZeroRateSeconds = 0.0f;
    double NextManagerSearchSeconds = 0.0;
};
