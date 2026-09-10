// Copyright RadioGuesser. All Rights Reserved.

#include "Map/RGCesiumMapManager.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#include "CesiumWebMapTileServiceRasterOverlay.h"

ARGCesiumMapManager::ARGCesiumMapManager()
{
    // Ticking disabled — no per-frame work needed
    PrimaryActorTick.bCanEverTick = false;

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
        }
    }

    // Add MapTiler Natural Earth overlay to Cesium World Terrain
    // This replaces Bing Maps and has no watermark on the tiles themselves
    AddMapTilerOverlay();
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
    if (!GuessMarkerMesh) return;

    const FVector WorldPos = GeoToWorld(Coordinate);
    GuessMarkerMesh->SetWorldLocation(WorldPos);

    if (CesiumGeoreference)
    {
        const FRotator UpRot = CesiumGeoreference->TransformEastSouthUpRotatorToUnreal(
            FRotator::ZeroRotator, WorldPos);
        GuessMarkerMesh->SetWorldRotation(UpRot);
    }

    GuessMarkerMesh->SetVisibility(true);
}

void ARGCesiumMapManager::ShowRoundResult(FRGGeoCoordinate GuessCoord, FRGGeoCoordinate ActualCoord)
{
    PlaceGuessMarker(GuessCoord);

    if (ActualLocationMarkerMesh)
    {
        const FVector ActualPos = GeoToWorld(ActualCoord);
        ActualLocationMarkerMesh->SetWorldLocation(ActualPos);

        if (CesiumGeoreference)
        {
            const FRotator UpRot = CesiumGeoreference->TransformEastSouthUpRotatorToUnreal(
                FRotator::ZeroRotator, ActualPos);
            ActualLocationMarkerMesh->SetWorldRotation(UpRot);
        }

        ActualLocationMarkerMesh->SetVisibility(true);
    }
}

void ARGCesiumMapManager::ClearRoundOverlays()
{
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

    // MapTiler Natural Earth — beautiful political map, no watermark, free tier
    // URL template: https://api.maptiler.com/maps/natural-earth/{z}/{x}/{y}.png?key=KEY
    const FString MapTilerKey = TEXT("Gyf1PzWCtsfSGE86susz");
    const FString TileUrl = FString::Printf(
        TEXT("https://api.maptiler.com/maps/basic-v2/{z}/{x}/{y}.png?key=%s"),
        *MapTilerKey);

    UCesiumWebMapTileServiceRasterOverlay* Overlay =
        NewObject<UCesiumWebMapTileServiceRasterOverlay>(
            Tileset,
            UCesiumWebMapTileServiceRasterOverlay::StaticClass(),
            TEXT("MapTilerOverlay"));

    if (!Overlay)
    {
        UE_LOG(LogMap, Warning, TEXT("AddMapTilerOverlay: Failed to create overlay object."));
        return;
    }

    Overlay->BaseUrl        = TileUrl;
    Overlay->Layer          = TEXT("");
    Overlay->Style          = TEXT("default");
    Overlay->TileMatrixSetID = TEXT("GoogleMapsCompatible");
    Overlay->Format         = TEXT("image/png");
    Overlay->RegisterComponent();

    UE_LOG(LogMap, Log, TEXT("AddMapTilerOverlay: MapTiler Natural Earth overlay added."));
}
