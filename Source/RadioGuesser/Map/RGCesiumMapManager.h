// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/RGMapSubsystem.h"
#include "RGCesiumMapManager.generated.h"

// Forward declarations
class ACesiumGeoreference;
class ACesium3DTileset;
class UCesiumRasterOverlay;
class UCesiumUrlTemplateRasterOverlay;
class UCesiumWebMapTileServiceRasterOverlay;
class UInstancedStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMapClick,
    FRGGeoCoordinate, Coordinate,
    FVector,          WorldHitPosition);

/**
 * ARGCesiumMapManager
 *
 * Placed once in the WorldMap level.
 * Wraps ACesiumGeoreference for WGS84 <-> Unreal coordinate conversion.
 * Manages guess and result markers on the globe.
 */
UCLASS(BlueprintType, Blueprintable)
class RADIOGUESSER_API ARGCesiumMapManager : public AActor
{
    GENERATED_BODY()

public:
    ARGCesiumMapManager();

    // No Tick needed — ticking is disabled in the constructor
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ── Coordinate conversion ─────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Map|Coordinates")
    FVector GeoToWorld(FRGGeoCoordinate Coordinate) const;

    UFUNCTION(BlueprintPure, Category = "Map|Coordinates")
    FRGGeoCoordinate WorldToGeo(FVector WorldPosition) const;

    // ── Map interaction ───────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Map")
    void HandleMapClick(FVector WorldHitPosition);

    UFUNCTION(BlueprintCallable, Category = "Map")
    void PlaceGuessMarker(FRGGeoCoordinate Coordinate);

    UFUNCTION(BlueprintCallable, Category = "Map")
    void ShowRoundResult(FRGGeoCoordinate GuessCoord, FRGGeoCoordinate ActualCoord);

    UFUNCTION(BlueprintCallable, Category = "Map")
    void ClearRoundOverlays();

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Map")
    FOnMapClick OnMapClick;

    // ── References (set in editor or auto-found in BeginPlay) ─────────────────

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Map|Setup")
    TObjectPtr<ACesiumGeoreference> CesiumGeoreference;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Markers")
    TObjectPtr<UStaticMeshComponent> GuessMarkerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Markers")
    TObjectPtr<UStaticMeshComponent> ActualLocationMarkerMesh;

    /** Dotted great-circle between guess and actual station (result phase). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Markers")
    TObjectPtr<UInstancedStaticMeshComponent> ResultArcMesh;

    /** Height above the globe surface for markers, in centimetres (500 m default) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Markers")
    float MarkerHeightOffset = 50000.0f;

private:
    UFUNCTION() void OnGuessPlaced(FRGGeoCoordinate Coordinate);
    UFUNCTION() void OnGuessResult(FRGGuessResult   Result);
    UFUNCTION() void OnGuessCleared();

    void ConfigureTileset(ACesium3DTileset* Tileset);
    void AddMapTilerOverlay();
    void ConfigureGlobeLighting();
    void EnsureMarkerMeshes();
    void UpdateMarkerTransform(UStaticMeshComponent* Mesh, FRGGeoCoordinate Coordinate);
    void UpdateMarkerScales();
    void UpdateResultArc();
    ACesium3DTileset* FindTileset() const;

    // Called 1 second after BeginPlay to re-apply tileset settings after
    // Cesium finishes its own BeginPlay initialization.
    UFUNCTION() void DeferredConfigureTileset();

    FTimerHandle DeferredConfigureTimer;

    FRGGeoCoordinate LastGuessCoord;
    FRGGeoCoordinate LastActualCoord;
    bool bGuessMarkerOn  = false;
    bool bActualMarkerOn = false;
};
