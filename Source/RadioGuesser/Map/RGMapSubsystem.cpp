// Copyright RadioGuesser. All Rights Reserved.

#include "Map/RGMapSubsystem.h"
#include "RadioGuesser.h"

// Haversine constants
static constexpr double RG_EARTH_RADIUS_KM = 6371.0;

void URGMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogMap, Log, TEXT("RGMapSubsystem initialised"));
}

void URGMapSubsystem::Deinitialize()
{
    UE_LOG(LogMap, Log, TEXT("RGMapSubsystem deinitialised"));
    Super::Deinitialize();
}

// ─── Guess flow ───────────────────────────────────────────────────────────────

void URGMapSubsystem::PlaceGuess(FRGGeoCoordinate Coordinate)
{
    PendingGuess     = Coordinate;
    bHasPendingGuess = true;
    UE_LOG(LogMap, Log, TEXT("Guess placed: lat=%.4f lon=%.4f"), Coordinate.Latitude, Coordinate.Longitude);
    OnGuessPlaced.Broadcast(Coordinate);
}

void URGMapSubsystem::ClearGuess()
{
    PendingGuess     = FRGGeoCoordinate{};
    bHasPendingGuess = false;
}

void URGMapSubsystem::ShowResult(const FRGGuessResult& Result)
{
    UE_LOG(LogMap, Log, TEXT("Round result — distance: %.1f km, score: %d"), Result.DistanceKm, Result.Score);
    OnGuessResult.Broadcast(Result);
}

// ─── Coordinate utilities ─────────────────────────────────────────────────────

FVector2D URGMapSubsystem::GeoToUnreal(FRGGeoCoordinate Coordinate,
                                        FRGGeoCoordinate Origin,
                                        float            MetersPerUnit)
{
    // Equirectangular projection — placeholder until Cesium CesiumGeoreference is integrated.
    // With Cesium, this will delegate to ACesiumGeoreference::TransformLongitudeLatitudeHeightToUnreal.
    const double LatRadOrigin = FMath::DegreesToRadians(Origin.Latitude);
    const double DeltaLat     = FMath::DegreesToRadians(Coordinate.Latitude  - Origin.Latitude);
    const double DeltaLon     = FMath::DegreesToRadians(Coordinate.Longitude - Origin.Longitude);

    const double X = RG_EARTH_RADIUS_KM * 1000.0 * DeltaLon * FMath::Cos(LatRadOrigin);
    const double Y = RG_EARTH_RADIUS_KM * 1000.0 * DeltaLat;

    // Convert metres → Unreal units (1 UU = MetersPerUnit metres)
    return FVector2D(
        static_cast<float>(X / MetersPerUnit),
        static_cast<float>(Y / MetersPerUnit)
    );
}

float URGMapSubsystem::HaversineDistanceKm(FRGGeoCoordinate A, FRGGeoCoordinate B)
{
    const double Lat1 = FMath::DegreesToRadians(A.Latitude);
    const double Lat2 = FMath::DegreesToRadians(B.Latitude);
    const double DLat = FMath::DegreesToRadians(B.Latitude  - A.Latitude);
    const double DLon = FMath::DegreesToRadians(B.Longitude - A.Longitude);

    const double SinDLat = FMath::Sin(DLat * 0.5);
    const double SinDLon = FMath::Sin(DLon * 0.5);

    const double H = SinDLat * SinDLat
                   + FMath::Cos(Lat1) * FMath::Cos(Lat2) * SinDLon * SinDLon;

    const double C = 2.0 * FMath::Atan2(FMath::Sqrt(H), FMath::Sqrt(1.0 - H));

    return static_cast<float>(RG_EARTH_RADIUS_KM * C);
}
