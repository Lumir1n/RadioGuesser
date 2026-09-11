// Copyright RadioGuesser. All Rights Reserved.

#include "Map/RGCesiumMapManager.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#include "CesiumRasterOverlay.h"
#include "CesiumUrlTemplateRasterOverlay.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Player/RGGlobePawn.h"

ARGCesiumMapManager::ARGCesiumMapManager()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    GuessMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuessMarker"));
    GuessMarkerMesh->SetupAttachment(Root);
    GuessMarkerMesh->SetVisibility(false);
    GuessMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ActualLocationMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ActualLocationMarker"));
    ActualLocationMarkerMesh->SetupAttachment(Root);
    ActualLocationMarkerMesh->SetVisibility(false);
    ActualLocationMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARGCesiumMapManager::BeginPlay()
{
    Super::BeginPlay();

    // Auto-find CesiumGeoreference via iterator (avoids GetActorOfClass type issues)
    if (!CesiumGeoreference)
    {
        for (TActorIterator<ACesiumGeoreference> It(GetWorld()); It; ++It)
        {
            CesiumGeoreference = *It;
            UE_LOG(LogMap, Log, TEXT("ARGCesiumMapManager: Found CesiumGeoreference automatically"));
            break;
        }
        if (!CesiumGeoreference)
        {
            UE_LOG(LogMap, Warning, TEXT("ARGCesiumMapManager: No CesiumGeoreference in level."));
        }
    }

    // Subscribe to MapSubsystem events
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMapSubsystem* MapSub = GI->GetSubsystem<URGMapSubsystem>())
        {
            MapSub->OnGuessPlaced.AddDynamic(this, &ARGCesiumMapManager::OnGuessPlaced);
            MapSub->OnGuessResult.AddDynamic(this, &ARGCesiumMapManager::OnGuessResult);
            MapSub->OnGuessCleared.AddDynamic(this, &ARGCesiumMapManager::OnGuessCleared);
        }
    }

    // Add MapTiler Natural Earth overlay to Cesium World Terrain
    // This replaces Bing Maps and has no watermark on the tiles themselves
    AddMapTilerOverlay();
    ConfigureGlobeLighting();
    EnsureMarkerMeshes();
}

void ARGCesiumMapManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bGuessMarkerOn)
    {
        UpdateMarkerTransform(GuessMarkerMesh, LastGuessCoord);
    }
    if (bActualMarkerOn)
    {
        UpdateMarkerTransform(ActualLocationMarkerMesh, LastActualCoord);
    }
}

// ─── Coordinate conversion ────────────────────────────────────────────────────

FVector ARGCesiumMapManager::GeoToWorld(FRGGeoCoordinate Coordinate) const
{
    if (!CesiumGeoreference)
    {
        UE_LOG(LogMap, Warning, TEXT("GeoToWorld: CesiumGeoreference is null"));
        return FVector::ZeroVector;
    }

    // Cesium 2.x API: (Longitude, Latitude, HeightMetres)
    const FVector LLH(
        Coordinate.Longitude,
        Coordinate.Latitude,
        MarkerHeightOffset / 100.0f   // cm → m
    );
    return CesiumGeoreference->TransformLongitudeLatitudeHeightPositionToUnreal(LLH);
}

FRGGeoCoordinate ARGCesiumMapManager::WorldToGeo(FVector WorldPosition) const
{
    if (!CesiumGeoreference)
    {
        UE_LOG(LogMap, Warning, TEXT("WorldToGeo: CesiumGeoreference is null"));
        return FRGGeoCoordinate{};
    }

    const FVector LLH = CesiumGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(WorldPosition);

    FRGGeoCoordinate Coord;
    Coord.Longitude = LLH.X;   // X = Longitude (degrees)
    Coord.Latitude  = LLH.Y;   // Y = Latitude  (degrees)
    return Coord;
}

// ─── Map interaction ──────────────────────────────────────────────────────────

void ARGCesiumMapManager::HandleMapClick(FVector WorldHitPosition)
{
    const FRGGeoCoordinate Coord = WorldToGeo(WorldHitPosition);
    UE_LOG(LogMap, Log, TEXT("Map click: lat=%.4f lon=%.4f"), Coord.Latitude, Coord.Longitude);

    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMapSubsystem* MapSub = GI->GetSubsystem<URGMapSubsystem>())
        {
            MapSub->PlaceGuess(Coord);
        }
    }
    OnMapClick.Broadcast(Coord, WorldHitPosition);
}

void ARGCesiumMapManager::PlaceGuessMarker(FRGGeoCoordinate Coordinate)
{
    LastGuessCoord = Coordinate;
    bGuessMarkerOn = true;
    UpdateMarkerTransform(GuessMarkerMesh, Coordinate);
    if (GuessMarkerMesh)
    {
        GuessMarkerMesh->SetVisibility(true);
    }
}

void ARGCesiumMapManager::ShowRoundResult(FRGGeoCoordinate GuessCoord, FRGGeoCoordinate ActualCoord)
{
    PlaceGuessMarker(GuessCoord);

    LastActualCoord = ActualCoord;
    bActualMarkerOn = true;
    UpdateMarkerTransform(ActualLocationMarkerMesh, ActualCoord);
    if (ActualLocationMarkerMesh)
    {
        ActualLocationMarkerMesh->SetVisibility(true);
    }

    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        if (ARGGlobePawn* Globe = Cast<ARGGlobePawn>(Pawn))
        {
            const float DistKm = URGMapSubsystem::HaversineDistanceKm(GuessCoord, ActualCoord);
            Globe->FocusOnGuessResult(GuessCoord, ActualCoord, DistKm);
        }
    }
}

void ARGCesiumMapManager::ClearRoundOverlays()
{
    bGuessMarkerOn  = false;
    bActualMarkerOn = false;
    if (GuessMarkerMesh)          GuessMarkerMesh->SetVisibility(false);
    if (ActualLocationMarkerMesh) ActualLocationMarkerMesh->SetVisibility(false);
}

// ─── MapSubsystem callbacks ───────────────────────────────────────────────────

void ARGCesiumMapManager::OnGuessPlaced(FRGGeoCoordinate Coordinate)
{
    PlaceGuessMarker(Coordinate);
}

void ARGCesiumMapManager::OnGuessResult(FRGGuessResult Result)
{
    ShowRoundResult(Result.PlayerGuess, Result.ActualLocation);
}

void ARGCesiumMapManager::OnGuessCleared()
{
    ClearRoundOverlays();
}

void ARGCesiumMapManager::UpdateMarkerTransform(UStaticMeshComponent* Mesh, FRGGeoCoordinate Coordinate)
{
    if (!Mesh)
    {
        return;
    }

    const FVector WorldPos = GeoToWorld(Coordinate);
    Mesh->SetWorldLocation(WorldPos);

    if (CesiumGeoreference)
    {
        const FRotator UpRot = CesiumGeoreference->TransformEastSouthUpRotatorToUnreal(
            FRotator::ZeroRotator, WorldPos);
        Mesh->SetWorldRotation(UpRot);
    }
}

void ARGCesiumMapManager::EnsureMarkerMeshes()
{
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!Sphere)
    {
        return;
    }

    auto SetupMarker = [Sphere](UStaticMeshComponent* Mesh, const FLinearColor& Color, float Scale)
    {
        if (!Mesh)
        {
            return;
        }
        if (!Mesh->GetStaticMesh())
        {
            Mesh->SetStaticMesh(Sphere);
        }
        Mesh->SetWorldScale3D(FVector(Scale));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(
                nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0, Base))
            {
                MID->SetVectorParameterValue(TEXT("Color"), Color);
            }
        }
    };

    SetupMarker(GuessMarkerMesh, FLinearColor(1.0f, 0.85f, 0.1f), 800.0f);
    SetupMarker(ActualLocationMarkerMesh, FLinearColor(1.0f, 0.15f, 0.1f), 800.0f);
}

void ARGCesiumMapManager::ConfigureGlobeLighting()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (AWorldSettings* WS = World->GetWorldSettings())
    {
        WS->bEnableWorldBoundsChecks = false;
    }

    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        if (ULightComponent* Light = It->GetLightComponent())
        {
            Light->SetCastShadows(false);
            Light->SetIntensity(12.0f);
        }
    }

    ASkyLight* Sky = nullptr;
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        Sky = *It;
        break;
    }

    if (!Sky)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Sky = World->SpawnActor<ASkyLight>(Params);
    }

    if (Sky)
    {
        if (USkyLightComponent* SkyComp = Sky->GetLightComponent())
        {
            SkyComp->SetMobility(EComponentMobility::Movable);
            SkyComp->bLowerHemisphereIsBlack = false;
            SkyComp->LowerHemisphereColor = FLinearColor::White;
            SkyComp->SetIntensity(6.0f);
            SkyComp->RecaptureSky();
        }
    }
}

// ─── MapTiler overlay ─────────────────────────────────────────────────────────

void ARGCesiumMapManager::AddMapTilerOverlay()
{
    // Find Cesium World Terrain tileset in the level
    ACesium3DTileset* Tileset = nullptr;
    for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It)
    {
        Tileset = *It;
        break;
    }

    if (!Tileset)
    {
        UE_LOG(LogMap, Warning, TEXT("AddMapTilerOverlay: No ACesium3DTileset found in level."));
        return;
    }

    // ── Step 1: Remove ALL existing raster overlay components from the tileset.
    //    This kills the Bing Maps overlay (and any others) that were saved in
    //    the .umap file. Without this step they fire "Could not parse web map
    //    service XML" on every load and the Bing Maps watermark stays visible.
    {
        TArray<UCesiumRasterOverlay*> ExistingOverlays;
        Tileset->GetComponents<UCesiumRasterOverlay>(ExistingOverlays);

        for (UCesiumRasterOverlay* OldOverlay : ExistingOverlays)
        {
            UE_LOG(LogMap, Log, TEXT("AddMapTilerOverlay: Removing old overlay '%s'"),
                *OldOverlay->GetName());
            OldOverlay->Deactivate();
            OldOverlay->DestroyComponent();
        }
        UE_LOG(LogMap, Log, TEXT("AddMapTilerOverlay: Removed %d existing overlay(s)"),
            ExistingOverlays.Num());
    }

    // ── Step 2: Add MapTiler Basic-v2 raster tiles.
    //    URL format: /maps/{style}/256/{z}/{x}/{y}.png — the 256/ prefix is
    //    mandatory. Without it MapTiler returns a vector style JSON document
    //    instead of PNG images, which Cesium cannot render.
    const FString MapTilerKey = TEXT("Gyf1PzWCtsfSGE86susz");
    const FString TileUrl = FString::Printf(
        TEXT("https://api.maptiler.com/maps/basic-v2/256/{z}/{x}/{y}.png?key=%s"),
        *MapTilerKey);

    UCesiumUrlTemplateRasterOverlay* Overlay =
        NewObject<UCesiumUrlTemplateRasterOverlay>(
            Tileset,
            UCesiumUrlTemplateRasterOverlay::StaticClass(),
            TEXT("MapTilerOverlay"));

    if (!Overlay)
    {
        UE_LOG(LogMap, Warning, TEXT("AddMapTilerOverlay: Failed to create overlay object."));
        return;
    }

    Overlay->TemplateUrl = TileUrl;

    // Cesium's {y} in WebMercator projection: y=0 = northernmost tile.
    // MapTiler Basic-v2 also uses y=0 = north (XYZ/Slippy Map convention).
    // They match — use {y} directly.
    Overlay->Projection = ECesiumUrlTemplateRasterOverlayProjection::WebMercator;
    Overlay->TileWidth  = 256;
    Overlay->TileHeight = 256;
    Overlay->MinimumLevel = 0;
    Overlay->MaximumLevel = 19;
    Overlay->bAutoActivate = true;
    Overlay->RegisterComponent();
    Tileset->AddInstanceComponent(Overlay);

    // Force the tileset to reload so newly added overlay is applied to all
    // tiles that were already loaded before BeginPlay ran.
    Tileset->RefreshTileset();

    UE_LOG(LogMap, Log, TEXT("AddMapTilerOverlay: MapTiler overlay added — URL: %s"), *TileUrl);
}
