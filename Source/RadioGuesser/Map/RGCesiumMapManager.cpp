// Copyright RadioGuesser. All Rights Reserved.

#include "Map/RGCesiumMapManager.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CesiumGeoreference.h"

ARGCesiumMapManager::ARGCesiumMapManager()
{
    PrimaryActorTick.bCanEverTick = false;  // No per-frame work needed

    // Root component — markers attach to this
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

    // Auto-find CesiumGeoreference in the level if not set in editor
    if (!CesiumGeoreference)
    {
        // GetActorOfClass takes TSubclassOf<AActor> — StaticClass() satisfies this implicitly
        AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), ACesiumGeoreference::StaticClass());
        CesiumGeoreference = Cast<ACesiumGeoreference>(Found);

        if (!CesiumGeoreference)
        {
            UE_LOG(LogMap, Warning,
                TEXT("ARGCesiumMapManager: No CesiumGeoreference in level. Assign it in Details."));
        }
        else
        {
            UE_LOG(LogMap, Log, TEXT("ARGCesiumMapManager: Found CesiumGeoreference automatically"));
        }
    }

    // Subscribe to MapSubsystem events
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMapSubsystem* MapSub = GI->GetSubsystem<URGMapSubsystem>())
        {
            MapSub->OnGuessPlaced.AddDynamic(this, &ARGCesiumMapManager::OnGuessPlaced);
            MapSub->OnGuessResult.AddDynamic(this, &ARGCesiumMapManager::OnGuessResult);
            UE_LOG(LogMap, Log, TEXT("ARGCesiumMapManager subscribed to MapSubsystem events"));
        }
    }
}

// ─── Coordinate conversion ─────────────────────────────────────────────────────
// Uses Cesium for Unreal 2.x API (no glm types exposed to user code)

FVector ARGCesiumMapManager::GeoToWorld(FRGGeoCoordinate Coordinate) const
{
    if (!CesiumGeoreference)
    {
        UE_LOG(LogMap, Warning, TEXT("GeoToWorld: CesiumGeoreference is null"));
        return FVector::ZeroVector;
    }

    // Cesium 2.x: TransformLongitudeLatitudeHeightPositionToUnreal(FVector(Lon, Lat, HeightM))
    // Returns position in the Georeference's local (not world) space.
    // We then convert to world space via the actor's transform.
    const FVector LLH(Coordinate.Longitude, Coordinate.Latitude, MarkerHeightOffset / 100.0);
    const FVector LocalPos = CesiumGeoreference->TransformLongitudeLatitudeHeightPositionToUnreal(LLH);

    // The georeference local space IS Unreal world space when georeference is at world origin
    return LocalPos;
}

FRGGeoCoordinate ARGCesiumMapManager::WorldToGeo(FVector WorldPosition) const
{
    if (!CesiumGeoreference)
    {
        UE_LOG(LogMap, Warning, TEXT("WorldToGeo: CesiumGeoreference is null"));
        return FRGGeoCoordinate{};
    }

    // Cesium 2.x: TransformUnrealPositionToLongitudeLatitudeHeight(FVector WorldPos)
    const FVector LLH = CesiumGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(WorldPosition);

    FRGGeoCoordinate Coord;
    Coord.Longitude = LLH.X;  // X = Longitude
    Coord.Latitude  = LLH.Y;  // Y = Latitude
    return Coord;
}

// ─── Map interaction ──────────────────────────────────────────────────────────

void ARGCesiumMapManager::HandleMapClick(FVector WorldHitPosition)
{
    const FRGGeoCoordinate Coord = WorldToGeo(WorldHitPosition);
    UE_LOG(LogMap, Log, TEXT("Map clicked: lat=%.4f lon=%.4f"), Coord.Latitude, Coord.Longitude);

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

    // Orient the marker so it stands upright on the globe surface
    if (CesiumGeoreference)
    {
        // TransformEastSouthUpRotatorToUnreal with identity gives us the "up" orientation at this point
        const FRotator UpRot = CesiumGeoreference->TransformEastSouthUpRotatorToUnreal(
            FRotator::ZeroRotator,
            WorldPos
        );
        GuessMarkerMesh->SetWorldRotation(UpRot);
    }

    GuessMarkerMesh->SetVisibility(true);
}

void ARGCesiumMapManager::ShowRoundResult(FRGGeoCoordinate GuessCoord, FRGGeoCoordinate ActualCoord)
{
    PlaceGuessMarker(GuessCoord);

    if (ActualLocationMarkerMesh)
    {
        const FVector ActualWorldPos = GeoToWorld(ActualCoord);
        ActualLocationMarkerMesh->SetWorldLocation(ActualWorldPos);

        if (CesiumGeoreference)
        {
            const FRotator UpRot = CesiumGeoreference->TransformEastSouthUpRotatorToUnreal(
                FRotator::ZeroRotator,
                ActualWorldPos
            );
            ActualLocationMarkerMesh->SetWorldRotation(UpRot);
        }

        ActualLocationMarkerMesh->SetVisibility(true);
    }

    UE_LOG(LogMap, Log, TEXT("Round result overlays shown"));
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
