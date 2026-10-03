// Copyright RadioGuesser. All Rights Reserved.

#include "Map/RGCesiumMapManager.h"
#include "RadioGuesser.h"
#include "Map/RGMapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "CesiumCreditSystem.h"
#include "Components/Widget.h"
#include "Styling/SlateTypes.h"
#include "ScreenCreditsWidget.h"
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#include "CesiumRasterOverlay.h"
#include "CesiumUrlTemplateRasterOverlay.h"
#include "CesiumWebMapTileServiceRasterOverlay.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Player/RGGlobePawn.h"
#include "Camera/PlayerCameraManager.h"

// MapTiler API key — shared key used for both terrain and map tiles.
// Free tier: 100 000 tiles/month — plenty for development.
static const FString MapTilerKey = TEXT("Gyf1PzWCtsfSGE86susz");

// MapTiler terrain tileset JSON — quantized-mesh format, same as Cesium World
// Terrain but served by MapTiler. No Cesium ion watermark.
static const FString MapTilerTerrainUrl =
    TEXT("https://api.maptiler.com/tiles/terrain-quantized-mesh-v2/tiles.json?key=Gyf1PzWCtsfSGE86susz");

// MapTiler streets-v2 WMTS URL template used for the raster overlay.
// {TileMatrix}/{TileCol}/{TileRow} — WMTS convention (same zoom/x/y as XYZ
// but named differently). Cesium's WMTS overlay handles these automatically.
static const FString MapTilerWmtsUrl =
    TEXT("https://api.maptiler.com/maps/streets-v2/{TileMatrix}/{TileCol}/{TileRow}.jpg?key=Gyf1PzWCtsfSGE86susz");

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

    // Find all Cesium3DTilesets. Keep exactly ONE (the first with terrain/world data),
    // hide all others. Multiple active tilesets cause the duplicate-continent artifact
    // (two separate LOD layers rendered at the same time).
    {
        TArray<ACesium3DTileset*> AllTilesets;
        for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It)
            AllTilesets.Add(*It);

        UE_LOG(LogMap, Log, TEXT("ARGCesiumMapManager: Found %d tileset(s) in level — keeping first, hiding rest"),
            AllTilesets.Num());

        for (int32 i = 0; i < AllTilesets.Num(); ++i)
        {
            ACesium3DTileset* T = AllTilesets[i];
            if (i == 0)
            {
                // This is our primary tileset — configure and keep visible.
                T->SetActorHiddenInGame(false);
                T->SetActorEnableCollision(true);
                ConfigureTileset(T);
                T->RefreshTileset();
            }
            else
            {
                // Extra tilesets cause duplicate rendering — hide them completely.
                T->SetActorHiddenInGame(true);
                T->SetActorEnableCollision(false);
                T->SuspendUpdate = true;
                UE_LOG(LogMap, Log, TEXT("ARGCesiumMapManager: Hiding extra tileset '%s'"), *T->GetName());
            }
        }
    }
    AddMapTilerOverlay();
    ConfigureGlobeLighting();
    EnsureMarkerMeshes();

    // Re-apply after 1 second in case Cesium's own BeginPlay restores stale values
    GetWorldTimerManager().SetTimer(
        DeferredConfigureTimer, this,
        &ARGCesiumMapManager::DeferredConfigureTileset,
        1.0f, false);

    // Hide Cesium credits widget after a short delay (it spawns asynchronously)
    FTimerHandle CreditsTimer;
    GetWorldTimerManager().SetTimer(
        CreditsTimer, this,
        &ARGCesiumMapManager::HideCesiumCredits,
        0.5f, false);
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

    // Scale markers with camera altitude so they stay visible from any zoom level.
    if (bGuessMarkerOn || bActualMarkerOn)
    {
        UpdateMarkerScales();
    }

    // Cesium's CreditSystem re-adds its widget every tick.
    // We collapse it every tick to counteract that.
    if (ACesiumCreditSystem* Credits = ACesiumCreditSystem::GetDefaultCreditSystem(this))
    {
        if (UScreenCreditsWidget* W = Credits->CreditsWidget)
        {
            if (W->GetVisibility() != ESlateVisibility::Collapsed)
                W->SetVisibility(ESlateVisibility::Collapsed);
        }
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

void ARGCesiumMapManager::UpdateMarkerScales()
{
    // Scale markers proportionally to camera altitude so they stay visible at any zoom.
    // Base scale: 10 000 (guess), 30 000 (actual). Multiply 1–4× with altitude.
    APlayerCameraManager* CamMgr = nullptr;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        CamMgr = PC->PlayerCameraManager;

    if (!CamMgr) return;

    const FVector CamPos = CamMgr->GetCameraLocation();
    const double  AltCm  = CamPos.Size();
    const double  AltKm  = AltCm * 0.00001;

    // [200 km, 8000 km] → multiplier [1.0, 4.0]
    const double ClampedKm  = FMath::Clamp(AltKm, 200.0, 8000.0);
    const float  Multiplier = static_cast<float>(FMath::GetMappedRangeValueClamped(
        FVector2D(200.0, 8000.0), FVector2D(1.0, 4.0), ClampedKm));

    if (bGuessMarkerOn  && GuessMarkerMesh)
        GuessMarkerMesh->SetWorldScale3D(FVector(15000.0f * Multiplier));
    if (bActualMarkerOn && ActualLocationMarkerMesh)
        ActualLocationMarkerMesh->SetWorldScale3D(FVector(15000.0f * Multiplier));
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
        Mesh->SetCastShadow(false);

        // EmissiveMeshMaterial — Unlit emissive material with an EmissiveColor param.
        // Works reliably in UE5 Substrate mode without needing BaseColor/Color params.
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
        if (!Base)
        {
            // Final fallback: WorldGridMaterial (may be wrong colour but at least visible)
            Base = LoadObject<UMaterialInterface>(
                nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
        }
        if (Base)
        {
            UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0, Base);
            if (MID)
            {
                // EmissiveMeshMaterial parameters confirmed from uasset binary inspection:
                // "EmissiveColor" (vector) and "Color" (vector) are both present.
                // Set both with a high-intensity value so it glows visibly.
                const FLinearColor BrightColor = Color * 10.0f; // HDR intensity for emissive
                MID->SetVectorParameterValue(TEXT("EmissiveColor"), BrightColor);
                MID->SetVectorParameterValue(TEXT("Color"),         BrightColor);
            }
        }
    };

    // Yellow = player's guess, Red = actual radio station location.
    // Both same base scale 15000. Actual is same size as guess for fairness.
    SetupMarker(GuessMarkerMesh,          FLinearColor(1.0f, 0.85f, 0.05f), 15000.0f);
    SetupMarker(ActualLocationMarkerMesh, FLinearColor(1.0f, 0.10f, 0.05f), 15000.0f);
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

    for (TActorIterator<AExponentialHeightFog> FogIt(World); FogIt; ++FogIt)
    {
        if (UExponentialHeightFogComponent* FogComp =
            FogIt->FindComponentByClass<UExponentialHeightFogComponent>())
        {
            FogComp->SetFogDensity(0.0f);
            FogComp->SetVisibility(false);
        }
    }

    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        if (ULightComponent* Light = It->GetLightComponent())
        {
            Light->SetCastShadows(false);
            Light->SetIntensity(1.5f);  // slightly dimmer for natural map look
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
            SkyComp->SetIntensity(1.0f);  // slightly dimmer for natural map look
            SkyComp->RecaptureSky();
        }
    }
}

// ─── MapTiler terrain + overlay setup ────────────────────────────────────────

void ARGCesiumMapManager::AddMapTilerOverlay()
{
    // Get the active tileset (first one — extras are hidden in BeginPlay).
    ACesium3DTileset* Tileset = nullptr;
    for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It)
    {
        Tileset = *It;
        break;
    }

    if (!Tileset)
    {
        UE_LOG(LogMap, Warning, TEXT("AddMapTilerOverlay: No ACesium3DTileset found."));
        return;
    }

    // ── Step 1: Switch terrain source from Cesium ion → MapTiler ─────────────
    // SetTilesetSource + SetUrl replaces the Cesium ion terrain (Asset ID 1)
    // with MapTiler's quantized-mesh terrain served from their CDN.
    // This eliminates the "CESIUM ion" watermark entirely.
    Tileset->SetTilesetSource(ETilesetSource::FromUrl);
    Tileset->SetUrl(MapTilerTerrainUrl);

    UE_LOG(LogMap, Log, TEXT("AddMapTilerOverlay: Tileset switched to MapTiler terrain URL"));

    // ── Step 2: Remove all existing raster overlays ───────────────────────────
    // The Bing Maps / Cesium ion overlays saved in the .umap must be removed
    // before we add ours; otherwise old overlays fire XML parse errors.
    {
        TArray<UCesiumRasterOverlay*> ExistingOverlays;
        Tileset->GetComponents<UCesiumRasterOverlay>(ExistingOverlays);
        for (UCesiumRasterOverlay* OldOverlay : ExistingOverlays)
        {
            OldOverlay->Deactivate();
            OldOverlay->DestroyComponent();
        }
        UE_LOG(LogMap, Log, TEXT("AddMapTilerOverlay: Removed %d old overlay(s)"),
            ExistingOverlays.Num());
    }

    // ── Step 3: Add MapTiler streets-v2 via WMTS raster overlay ──────────────
    // MapTiler documentation recommends WMTS for Cesium for Unreal.
    // streets-v2 — colourful cartoonish style, strong country borders.
    UCesiumWebMapTileServiceRasterOverlay* Overlay =
        NewObject<UCesiumWebMapTileServiceRasterOverlay>(
            Tileset,
            UCesiumWebMapTileServiceRasterOverlay::StaticClass(),
            TEXT("MapTilerWmtsOverlay"));

    if (!Overlay)
    {
        UE_LOG(LogMap, Warning, TEXT("AddMapTilerOverlay: Failed to create WMTS overlay."));
        return;
    }

    // BaseUrl contains the full template including {TileMatrix}/{TileCol}/{TileRow}
    Overlay->BaseUrl    = MapTilerWmtsUrl;
    Overlay->Layer      = TEXT("");      // not needed for template-style URLs
    Overlay->Style      = TEXT("");
    Overlay->Format     = TEXT("image/jpeg");
    Overlay->TileMatrixSetID    = TEXT("");
    Overlay->Projection = ECesiumWebMapTileServiceRasterOverlayProjection::WebMercator;
    Overlay->bSpecifyZoomLevels = true;
    Overlay->MinimumLevel       = 0;
    Overlay->MaximumLevel       = 14;   // streets-v2 raster tiles cap at zoom 14
    Overlay->TileWidth          = 256;
    Overlay->TileHeight         = 256;
    Overlay->bAutoActivate      = true;
    Overlay->RegisterComponent();
    Tileset->AddInstanceComponent(Overlay);

    Tileset->RefreshTileset();

    UE_LOG(LogMap, Log, TEXT("AddMapTilerOverlay: WMTS overlay added (streets-v2, zoom 0-14)"));
}

void ARGCesiumMapManager::HideCesiumCredits()
{
    // Remove the Cesium credit/attribution widget from all viewports.
    // This hides the "CESIUM ion" logo and the on-screen attribution bar.
    //
    // Note: MapTiler/OpenStreetMap attribution is still legally required —
    // we keep it accessible in the in-game UI rather than the Cesium widget.
    // removeCreditsFromViewports() is the only public API to hide the widget.
    if (ACesiumCreditSystem* Credits =
            ACesiumCreditSystem::GetDefaultCreditSystem(this))
    {
        Credits->removeCreditsFromViewports();
        // Also hide the actor itself so it doesn't respawn the widget
        Credits->SetActorHiddenInGame(true);
        Credits->SetActorTickEnabled(false);
        UE_LOG(LogMap, Log, TEXT("HideCesiumCredits: credit widget removed"));
    }

    // Also make sure all tileset ShowCreditsOnScreen flags are off
    for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It)
    {
        (*It)->ShowCreditsOnScreen = false;
    }
}

void ARGCesiumMapManager::DeferredConfigureTileset()
{
    // Re-apply settings to only the active (first, non-hidden) tileset.
    // Also re-hide any extras in case they got re-enabled somehow.
    TArray<ACesium3DTileset*> AllTilesets;
    for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It)
        AllTilesets.Add(*It);

    for (int32 i = 0; i < AllTilesets.Num(); ++i)
    {
        ACesium3DTileset* T = AllTilesets[i];
        if (i == 0)
        {
            T->SetActorHiddenInGame(false);
            ConfigureTileset(T);
            T->RefreshTileset();
        }
        else
        {
            T->SetActorHiddenInGame(true);
            T->SetActorEnableCollision(false);
            T->SuspendUpdate = true;
        }
    }
    UE_LOG(LogMap, Log, TEXT("DeferredConfigureTileset: done, %d total, 1 active"), AllTilesets.Num());
}

ACesium3DTileset* ARGCesiumMapManager::FindTileset() const
{
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
        {
            return *It;
        }
    }
    return nullptr;
}

void ARGCesiumMapManager::ConfigureTileset(ACesium3DTileset* Tileset)
{
    if (!Tileset)
    {
        return;
    }

    // ── Mobility ──────────────────────────────────────────────────────────────
    // Origin rebasing (CesiumOriginShiftComponent on the pawn) moves the
    // tileset actor through Unreal space each time the origin is shifted.
    // The tileset must be Movable for that to work.
    Tileset->SetMobility(EComponentMobility::Movable);

    // ── ForbidHoles = FALSE ────────────────────────────────────────────────────
    // *** THIS IS THE KEY FIX FOR DUPLICATE CONTINENTS ***
    //
    // ForbidHoles = true causes Cesium to render BOTH a parent tile AND its
    // child tiles simultaneously while the child is loading. At orbit distance
    // (~5 000 km) the parent tile covers a continent; when Cesium starts
    // loading higher-resolution children it shows the parent AND the children
    // at the same time → two copies of Australia / Africa visible at once.
    //
    // With ForbidHoles = false (the Cesium default), Cesium shows only the
    // best available tile at any given moment. There may be a brief blank patch
    // while a child loads, but there will never be a doubled image.
    Tileset->ForbidHoles = false;

    // ── FogCulling = TRUE (Cesium default) ───────────────────────────────────
    // Fog culling drops tiles near/below the visual horizon based on camera
    // altitude. For an orbit camera this is exactly what we want: tiles on the
    // far side of the globe are below the "horizon" and get culled, preventing
    // them from appearing as ghost images through the planet.
    // The previous value of false was wrong and contributed to the artefacts.
    Tileset->EnableFogCulling = true;

    // ── FrustumCulling = TRUE (Cesium default) ───────────────────────────────
    // Frustum culling drops tiles outside the camera frustum. Combined with
    // fog culling, this means only tiles in the visible hemisphere are loaded.
    Tileset->EnableFrustumCulling = true;

    // ── ScreenSpaceError ──────────────────────────────────────────────────────
    // At 5 000 km altitude the entire Earth occupies maybe 800 px on screen.
    // With SSE=16 (default) Cesium loads far more detail than the screen can
    // show, wasting bandwidth and GPU. SSE=32 halves the tile count; SSE=64
    // is fine for a whole-globe view. We use 32 as a balance: still sharp
    // when zoomed in to country level, not overloaded at orbit altitude.
    Tileset->SetMaximumScreenSpaceError(32.0);

    // ── Preloading ────────────────────────────────────────────────────────────
    // PreloadAncestors helps when zooming out (parents are already cached).
    // PreloadSiblings causes tiles adjacent to the view to preload — this
    // doubles tile count for little benefit at globe scale. Disable it.
    Tileset->PreloadAncestors = true;
    Tileset->PreloadSiblings  = false;

    // ── Physics meshes ────────────────────────────────────────────────────────
    // We need physics meshes for raycasts (click-to-guess, drag-to-pan).
    Tileset->SetCreatePhysicsMeshes(true);

    // ── Credits widget ────────────────────────────────────────────────────────
    // Disable the on-screen "CESIUM ion" credit overlay on this tileset.
    // The credit system widget is handled separately by HideCesiumCredits().
    Tileset->ShowCreditsOnScreen = false;

    // Render tiles without engine lighting — keeps colours as MapTiler intended.
    // IgnoreKhrMaterialsUnlit=false means tiles respect their unlit material flag,
    // which makes them look the same as in a browser (no directional light baking).
    Tileset->SetIgnoreKhrMaterialsUnlit(false);

    UE_LOG(LogMap, Log,
        TEXT("ConfigureTileset: ForbidHoles=false FogCulling=true FrustumCulling=true SSE=32"));
}
