// HeldItemActor.cpp
#include "Player/HeldItemActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Config/HerbalistSettings.h"

AHeldItemActor::AHeldItemActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComponent;
    // Предмет в руке ни во что не упирается и не мешает трассам взгляда.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetCastShadow(false);
    // Размер заглушки -- горсть, не валун: базовые формы UE -- метровые.
    MeshComponent->SetRelativeScale3D(FVector(0.08f));

    // Движковые простые формы -- заглушки до арта. FObjectFinder в
    // конструкторе даёт жёсткую ссылку с CDO: кукер возьмёт их в сборку.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    SphereMesh = Sphere.Object;
    CubeMesh = Cube.Object;
    CylinderMesh = Cylinder.Object;
    BaseMaterial = Material.Object;
}

FLinearColor AHeldItemActor::ColorForState(const FRealState& Perceived)
{
    // Порча перекрывает всё: гнилое выглядит гнилым, куда бы ни тянуло.
    if (Perceived.Meta.Corruption >= 0.5f)
    {
        return FLinearColor(0.25f, 0.22f, 0.12f);
    }
    const FDirection& D = Perceived.Direction;
    const float Top = FMath::Max(FMath::Max(D.Body, D.Mind), FMath::Max(D.Spirit, D.Nature));
    // Та же палитра осей, что у окна карты состояния: Тело -- тёплое,
    // Разум -- холодное, Дух -- светлое, Природа -- зелёное.
    if (Top == D.Body)   return FLinearColor(0.75f, 0.35f, 0.2f);
    if (Top == D.Mind)   return FLinearColor(0.3f, 0.45f, 0.8f);
    if (Top == D.Spirit) return FLinearColor(0.85f, 0.82f, 0.6f);
    return FLinearColor(0.3f, 0.6f, 0.25f);
}

void AHeldItemActor::ShowItem(const FInventoryItem& Item, bool bIsMineral, bool bIsLiquid, UStaticMesh* ItemMesh)
{
    ShownItem = Item;
    ShownColor = ColorForState(Item.State);
    bShowingItemMesh = ItemMesh != nullptr;
    if (bShowingItemMesh)
    {
        MeshComponent->SetStaticMesh(ItemMesh);
        MeshComponent->EmptyOverrideMaterials();
        const float Longest = 2.0f * ItemMesh->GetBounds().BoxExtent.GetMax();
        MeshNormalize = Longest > 1.0f ? PlantSizeOverShape * 100.0f / Longest : 1.0f;
        ApplySize();
        return;
    }

    MeshNormalize = 1.0f;
    UStaticMesh* Mesh = bIsLiquid ? CylinderMesh.Get() : (bIsMineral ? CubeMesh.Get() : SphereMesh.Get());
    if (Mesh)
    {
        MeshComponent->SetStaticMesh(Mesh);
    }
    if (!TintMaterial && BaseMaterial)
    {
        TintMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
    }
    if (TintMaterial)
    {
        MeshComponent->SetMaterial(0, TintMaterial);
        TintMaterial->SetVectorParameterValue(TEXT("Color"), ShownColor);
    }
    ApplySize();
}

UStaticMesh* AHeldItemActor::FindItemMesh(const UObject* WorldContext, const FInventoryItem& Item)
{
    // Зелье и вода -- из настроек: строки с мешем у них нет (2026-09-28).
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    if (Item.IngredientID == FName(TEXT("Potion")))
    {
        return Settings ? Settings->PotionItemMesh.LoadSynchronous() : nullptr;
    }
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    const UIngredientRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    const FIngredientTableRow* Row = Registry ? Registry->GetRow(Item.IngredientID) : nullptr;
    if (Item.bIsWater || (Row && Row->Class == EIngredientClass::Water))
    {
        return Settings ? Settings->WaterItemMesh.LoadSynchronous() : nullptr;
    }
    return Row ? Row->ResourceMesh : nullptr;
}

void AHeldItemActor::SetShownSize(float Size)
{
    ShownSize = Size;
    ApplySize();
}

void AHeldItemActor::ApplySize()
{
    SetActorScale3D(FVector(ShownSize * MeshNormalize));
}

void AHeldItemActor::SetBrightness(float Brightness)
{
    if (TintMaterial)
    {
        FLinearColor Color = ShownColor * Brightness;
        Color.A = 1.0f;
        TintMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    }
}
