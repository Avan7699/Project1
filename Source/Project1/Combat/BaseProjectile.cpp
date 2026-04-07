#include "BaseProjectile.h"
#include "CombatComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ABaseProjectile::ABaseProjectile()
{
    // Collision sphere (root)
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(10.f);
    CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    CollisionComp->SetGenerateOverlapEvents(true);
    RootComponent = CollisionComp;

    // Mesh (visual only)
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(CollisionComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.2f));

    // Projectile movement
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = Speed;
    ProjectileMovement->MaxSpeed = Speed;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.f;
}

void ABaseProjectile::BeginPlay()
{
    Super::BeginPlay();
    SetLifeSpan(LifeSpanSeconds);
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Magenta, TEXT("Projectile BeginPlay fired!"));
    }

    // Bind overlap
    CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ABaseProjectile::OnOverlapBegin);

    // Apply speed (in case it was changed in editor)
    ProjectileMovement->InitialSpeed = Speed;
    ProjectileMovement->MaxSpeed = Speed;
}

void ABaseProjectile::SetOwnerUnit(AActor* OwnerUnit, float InDamage)
{
    InstigatorUnit = OwnerUnit;
    Damage = InDamage;
}

void ABaseProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // Not initialized yet — owner hasn't been set, ignore all overlaps
    if (!InstigatorUnit) return;

    // Ignore self and owner
    if (!OtherActor || OtherActor == this || OtherActor == InstigatorUnit) return;

    // Check if hit actor has a CombatComponent
    UCombatComponent* OtherCombat = OtherActor->FindComponentByClass<UCombatComponent>();
    if (!OtherCombat) return;

    // Check teams
    UCombatComponent* InstigatorCombat = nullptr;
    if (InstigatorUnit)
    {
        InstigatorCombat = InstigatorUnit->FindComponentByClass<UCombatComponent>();
    }

    // Don't hit friendlies
    if (InstigatorCombat && OtherCombat->TeamID == InstigatorCombat->TeamID) return;

    // Apply damage
    OtherCombat->ApplyDamage(Damage, InstigatorUnit);

    // Destroy projectile
    Destroy();
}