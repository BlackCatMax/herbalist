// Source/ProjectHerbalistTests/Private/Tests/TramplePlaneLayoutTest.cpp
//
// Раскладка UV движковой плоскости /Engine/BasicShapes/Plane (2026-09-12).
// M_RVTWriter читает текстуру тропы по UV0 плоскости, а FTrampleField кладёт
// тексели столбцом вдоль мировой X и строкой вдоль Y. Совпадают они, только
// если U идёт вдоль локальной X, а V -- вдоль Y. Снято зондом с меша:
// вершины (+-50, +-50) -> UV ((X+50)/100, (Y+50)/100). Прототип BP_PaintTest
// жаловался, что "UV0 у Plane не совпадает с мировыми X/Y" -- у самого меша
// совпадает, расходилось что-то в расстановке плоскости. Если меш когда-
// нибудь поменяется, этот тест упадёт раньше, чем тропы лягут зеркально.

#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrample_EnginePlaneUVFollowsLocalXY,
    "Herbalist.Trample.EnginePlaneUVFollowsLocalXY",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrample_EnginePlaneUVFollowsLocalXY::RunTest(const FString& Parameters)
{
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!TestNotNull(TEXT("Plane mesh loads"), Plane)) return false;

    const FStaticMeshRenderData* RenderData = Plane->GetRenderData();
    if (!TestTrue(TEXT("Render data present"), RenderData && RenderData->LODResources.Num() > 0)) return false;

    const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
    const int32 NumVerts = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
    TestTrue(TEXT("У плоскости есть вершины"), NumVerts > 0);

    for (int32 Index = 0; Index < NumVerts; ++Index)
    {
        const FVector3f Position = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index);
        const FVector2f UV = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Index, 0);
        TestEqual(FString::Printf(TEXT("v%d: U = (X + 50) / 100"), Index), UV.X, (Position.X + 50.0f) / 100.0f, 0.001f);
        TestEqual(FString::Printf(TEXT("v%d: V = (Y + 50) / 100"), Index), UV.Y, (Position.Y + 50.0f) / 100.0f, 0.001f);
    }

    TestEqual(TEXT("Плоскость 100 см по X"), static_cast<float>(Plane->GetBounds().GetBox().GetSize().X), 100.0f, 0.01f);
    TestEqual(TEXT("Плоскость 100 см по Y"), static_cast<float>(Plane->GetBounds().GetBox().GetSize().Y), 100.0f, 0.01f);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
