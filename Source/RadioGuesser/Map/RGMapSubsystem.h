// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RGMapSubsystem.generated.h"

/**
 * FRGGeoCoordinate
 * Geographic WGS84 coordinate pair (latitude / longitude in degrees).
 */
USTRUCT(BlueprintType)
struct RADIOGUESSER_API FRGGeoCoordinate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Latitude  = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Longitude = 0.0;
};

/**
 * FRGGuessResult
 * Filled after the server returns the authoritative round result.
 */
USTRUCT(BlueprintType)
struct RADIOGUESSER_API FRGGuessResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FRGGeoCoordinate PlayerGuess;
    UPROPERTY(BlueprintReadOnly) FRGGeoCoordinate ActualLocation;
    UPROPERTY(BlueprintReadOnly) float DistanceKm = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 Score      = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuessPlaced, FRGGeoCoordinate, Coordinate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuessResult, FRGGuessResult,    Result);

/**
 * URGMapSubsystem
 *
 * Manages the world map state:
 *  - current player guess marker
 *  - coordinate ↔ Unreal world conversion
 *  - result visualization trigger
 *
 * Actual geographic rendering is handled by the Map Actors/Components
 * (Cesium or custom pipeline) that listen to this subsystem's events.
 */
UCLASS()
class RADIOGUESSER_API URGMapSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Guess flow ────────────────────────────────────────────────────────────

    /** Called when the player clicks on the map to place a tentative guess */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void PlaceGuess(FRGGeoCoordinate Coordinate);

    /** Clear current tentative guess (e.g. before a new round) */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void ClearGuess();

    /** Returns true if a guess has been placed this round */
    UFUNCTION(BlueprintPure, Category = "Map")
    bool HasPendingGuess() const { return bHasPendingGuess; }

    UFUNCTION(BlueprintPure, Category = "Map")
    FRGGeoCoordinate GetPendingGuess() const { return PendingGuess; }

    /** Called by the match system when the server returns authoritative results */
    UFUNCTION(BlueprintCallable, Category = "Map")
    void ShowResult(const FRGGuessResult& Result);

    // ── Coordinate utilities ──────────────────────────────────────────────────

    /**
     * Convert WGS84 geographic coordinates to a flat Unreal world XY position.
     * Scale: 1 Unreal unit = 1 cm. Projection: equirectangular (placeholder;
     * will be replaced by Cesium georeferencing).
     */
    UFUNCTION(BlueprintPure, Category = "Map|Coordinates")
    static FVector2D GeoToUnreal(FRGGeoCoordinate Coordinate,
                                  FRGGeoCoordinate Origin,
                                  float            MetersPerUnit = 0.01f);

    /**
     * Compute great-circle distance in kilometres between two WGS84 coordinates
     * using the Haversine formula.  Server is authoritative for scored results;
     * this is for client-side preview only.
     */
    UFUNCTION(BlueprintPure, Category = "Map|Coordinates")
    static float HaversineDistanceKm(FRGGeoCoordinate A, FRGGeoCoordinate B);

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Map")
    FOnGuessPlaced OnGuessPlaced;

    UPROPERTY(BlueprintAssignable, Category = "Map")
    FOnGuessResult OnGuessResult;

private:
    UPROPERTY() bool             bHasPendingGuess = false;
    UPROPERTY() FRGGeoCoordinate PendingGuess;
};
