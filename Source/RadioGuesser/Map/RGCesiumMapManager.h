// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/RGMapSubsystem.h"
#include "RGCesiumMapManager.generated.h"

// Forward declarations — avoid pulling in all Cesium headers in this header
class ACesiumGeoreference;
class UCesiumGlobeAnchorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMapClick,
    FRGGeoCoordinate, Coordinate,
    FVector,          WorldHitPosition);

/**
 * ARGCesiumMapManager
 *
 * Placed once in the main gameplay level.
 * Wraps ACesiumGeoreference to provide:
 *  - WGS84 ↔ Unreal world coordinate conversion
 *  - Mouse-click → geographic coordinate
 *  - Guess marker placement and animation
 *  - Result visualization (actual location marker, distance line)
 *
 * Visual configuration (materials, meshes, scales) is done via Blueprint
 * subclass — this class only provides the C++ coordinate logic.
 */
UCLASS(BlueprintType, Blueprintable)
class RADIOGUESSER_API ARGCesiumMapManager : public AActor
{
    GENERATED_BODY()

public:
    ARGCesiumMapManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ── Coordinate conversion ─────────────────────────────────────────────────

    /**
     * Convert WGS84 (lat, lon, height=0) to Unreal Engine world position.
     * Delegates to the referenced CesiumGeoreference.
     */
    UFUNCTION(BlueprintPure, Category = "Map|Coordinates")
    FVector GeoToWorld(FRGGeoCoordinate Coordinate) const;

    /**
     * Convert Unreal world position to WGS84 coordinates.
     */
    UFUNCTION(BlueprintPure, Category = "Map|Coordinates")
    FRGGeoCoordinate WorldToGeo(FVector WorldPosition) const;

    // ── Map interaction ───────────────────────────────────────────────────────

    /**
     * Call this when the player clicks the globe.
     * Converts hit position to geographic coordinates and broadcasts OnMapClick.
     * Also updates the guess marker position.
     */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void HandleMapClick(FVector WorldHitPosition);

    /**
     * Show the guess marker at the given geographic coordinate.
     */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void PlaceGuessMarker(FRGGeoCoordinate Coordinate);

    /**
     * Show the actual station marker and draw the distance line.
     * Called after a round result is received.
     */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void ShowRoundResult(FRGGeoCoordinate GuessCoord, FRGGeoCoordinate ActualCoord);

    /**
     * Hide all round-specific overlays (called at round start).
     */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void ClearRoundOverlays();

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Map")
    FOnMapClick OnMapClick;

    // ── References ────────────────────────────────────────────────────────────

    /** Assign this in the level to the placed CesiumGeoreference actor */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Map|Setup")
    TObjectPtr<ACesiumGeoreference> CesiumGeoreference;

    /** Mesh component used for the player's guess marker */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Markers")
    TObjectPtr<UStaticMeshComponent> GuessMarkerMesh;

    /** Mesh component for the actual station location marker */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Markers")
    TObjectPtr<UStaticMeshComponent> ActualLocationMarkerMesh;

    /** Height above globe surface for markers, in centimetres (500m default) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Markers")
    float MarkerHeightOffset = 50000.0f;

private:
    UFUNCTION() void OnGuessPlaced(FRGGeoCoordinate Coordinate);
    UFUNCTION() void OnGuessResult(FRGGuessResult Result);
};
