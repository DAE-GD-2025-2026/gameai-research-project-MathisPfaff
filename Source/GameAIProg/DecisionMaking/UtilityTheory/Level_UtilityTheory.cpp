#include "Level_UtilityTheory.h"
#include "Kismet/GameplayStatics.h"

ALevel_UtilityTheory::ALevel_UtilityTheory()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ALevel_UtilityTheory::BeginPlay()
{
    Super::BeginPlay();

    // --- Spawn both agents ---
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    SeekAgent = GetWorld()->SpawnActor<ASteeringAgent>(
        SteeringAgentClass, FVector(-300, 0, 100), FRotator::ZeroRotator, Params);

    UtilityAgent = GetWorld()->SpawnActor<ASteeringAgent>(
        SteeringAgentClass, FVector(300, 0, 100), FRotator::ZeroRotator, Params);

    // SeekAgent always seeks the UtilityAgent
    if (SeekAgent)
        SeekAgent->SetSteeringBehavior(&m_SeekBehavior);

    // --- Wire up Utility Brain ---
    PursuitAction = Brain.AddAction(std::make_unique<UAPursuitAction>());
    EvadeAction   = Brain.AddAction(std::make_unique<UAEvadeAction>());
    WanderAction  = Brain.AddAction(std::make_unique<UAWanderAction>());

    if (SeekAgent)
    {
        SeekAgent->SetMaxLinearSpeed(SeekAgentSpeed);
    }
    if (UtilityAgent)
    {
        UtilityAgent->SetMaxLinearSpeed(UtilityAgentSpeed);
    }
}

void ALevel_UtilityTheory::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!SeekAgent || !UtilityAgent) return;
    
    // --- Left click sets the seek agent's target ---
    if (PlayerController && PlayerController->IsInputKeyDown(EKeys::LeftMouseButton))
    {
        SeekMouseTarget.Position = FVector2D(LatestMouseWorldPos.X, LatestMouseWorldPos.Y);
        m_SeekBehavior.SetTarget(SeekMouseTarget);
    }

    DrawImGui();

    // --- Freeze timer ---
    if (bUtilityAgentFrozen)
    {
        FreezeTimer -= DeltaTime;

        // Zero velocity every tick so steering can't creep back in
        UtilityAgent->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        UtilityAgent->GetCharacterMovement()->StopMovementImmediately();

        if (FreezeTimer <= 0.f)
            bUtilityAgentFrozen = false;

        return;  // skip AI update
    }

    CheckTagging();
    UpdateContexts();
    Brain.Update(*UtilityAgent, DeltaTime);
}

void ALevel_UtilityTheory::UpdateContexts()
{
    float RawDist = FVector::Dist(SeekAgent->GetActorLocation(), UtilityAgent->GetActorLocation());
    float NormDist = FMath::Clamp(RawDist / MaxRelevantDistance, 0.f, 1.f);

    // Update pursuit: target IS the seek agent (SeekAgent is chasing, utility agent pursues back if it becomes "it")
    FTargetData PursuitTarget;
    PursuitTarget.Position       = FVector2D(SeekAgent->GetActorLocation());
    PursuitTarget.LinearVelocity = SeekAgent->GetLinearVelocity();
    PursuitAction->SetTarget(PursuitTarget);
    PursuitAction->Context.NormalizedDistanceToPlayer = NormDist;
    PursuitAction->Context.bIsIt                     = !bSeekAgentIsIt; // utility agent is "it" when SeekAgent is not

    // Update evade: evade the seek agent
    FTargetData EvadeTarget;
    EvadeTarget.Position       = FVector2D(SeekAgent->GetActorLocation());
    EvadeTarget.LinearVelocity = SeekAgent->GetLinearVelocity();
    EvadeAction->SetTarget(EvadeTarget);
    EvadeAction->Context.NormalizedDistanceToSeeker = NormDist;
    EvadeAction->Context.bIsIt                     = !bSeekAgentIsIt;

    // Update wander
    WanderAction->Context.NormalizedDistanceToSeeker = NormDist;
}

void ALevel_UtilityTheory::CheckTagging()
{
    // Don't tag while frozen
    if (bUtilityAgentFrozen) return;

    float TagRadius = SeekAgent->GetCapsuleRadius() + UtilityAgent->GetCapsuleRadius() + 20.f;
    float Dist = FVector::Dist(SeekAgent->GetActorLocation(), UtilityAgent->GetActorLocation());

    if (Dist <= TagRadius)
    {
        bSeekAgentIsIt = !bSeekAgentIsIt;

        // Freeze the utility agent so it can't immediately re-tag
        bUtilityAgentFrozen = true;
        FreezeTimer = TagFreezeTime;

        // Stop movement during freeze
        UtilityAgent->GetCharacterMovement()->StopMovementImmediately();
        UtilityAgent->SetSteeringBehavior(nullptr);  // no behavior while frozen
    }
}

void ALevel_UtilityTheory::DrawImGui()
{
    ImGui::SetNextWindowPos(WindowPos);
    ImGui::SetNextWindowSize(WindowSize);
    ImGui::Begin("Utility Theory Debug");

    // =========================================================
    // SECTION: Settings  (add new agent/world sliders here)
    // =========================================================
    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::SliderFloat("SeekAgent Speed",    &SeekAgentSpeed,    100.f, 1000.f, "%.0f"))
        {
            if (SeekAgent) SeekAgent->SetMaxLinearSpeed(SeekAgentSpeed);
        }
        if (ImGui::SliderFloat("Utility Agent Speed", &UtilityAgentSpeed, 100.f, 1000.f, "%.0f"))
        {
            if (UtilityAgent) UtilityAgent->SetMaxLinearSpeed(UtilityAgentSpeed);
        }
        if (TrimWorld)
        {
            ImGui::Checkbox("World Trimming", &TrimWorld->bShouldTrimWorld);
        }
    }
    ImGui::Spacing();

    // =========================================================
    // SECTION: Tag State
    // =========================================================
    if (ImGui::CollapsingHeader("Tag State", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("SeekAgent is 'it':    %s", bSeekAgentIsIt  ? "YES" : "NO");
        ImGui::Text("UtilityAgent is 'it': %s", !bSeekAgentIsIt ? "YES" : "NO");
        if (bUtilityAgentFrozen)
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.f, 1.f), "FROZEN: %.1fs left", FreezeTimer);

        float Dist = FVector::Dist(SeekAgent->GetActorLocation(), UtilityAgent->GetActorLocation());
        ImGui::Text("Raw distance:  %.0f", Dist);
        ImGui::Text("Norm distance: %.2f", FMath::Clamp(Dist / MaxRelevantDistance, 0.f, 1.f));
    }

    ImGui::Spacing();

    // =========================================================
    // SECTION: Utility Scores  (don't touch this when adding settings)
    // =========================================================
    if (ImGui::CollapsingHeader("Utility Scores", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto DrawAction = [&](const char* ActionName, float TotalScore, bool bIsActive,
                              const std::vector<std::pair<std::string, float>>& ConsiderationScores)
        {
            if (bIsActive)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.f, 0.2f, 1.f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));

            bool bOpen = ImGui::TreeNode(ActionName);
            ImGui::PopStyleColor();

            ImGui::SameLine(120);
            ImVec4 BarColor = bIsActive ? ImVec4(0.2f, 0.9f, 0.2f, 1.f) : ImVec4(0.2f, 0.4f, 0.9f, 1.f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BarColor);
            char BarLabel[32];
            FCStringAnsi::Snprintf(BarLabel, sizeof(BarLabel), "%.2f", TotalScore);
            ImGui::ProgressBar(TotalScore, ImVec2(-1, 14), BarLabel);
            ImGui::PopStyleColor();

            if (bOpen)
            {
                ImGui::Indent(8.f);
                for (auto& [Name, Score] : ConsiderationScores)
                {
                    ImGui::Text("%-20s", Name.c_str());
                    ImGui::SameLine(140);
                    ImVec4 CColor = Score > 0.6f
                        ? ImVec4(0.2f, 1.f,  0.2f, 1.f)
                        : Score > 0.3f
                            ? ImVec4(1.f,  0.85f, 0.f,  1.f)
                            : ImVec4(1.f,  0.3f,  0.3f, 1.f);
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, CColor);
                    char CLabel[32];
                    FCStringAnsi::Snprintf(CLabel, sizeof(CLabel), "%.2f", Score);
                    ImGui::ProgressBar(Score, ImVec2(-1, 12), CLabel);
                    ImGui::PopStyleColor();
                }
                ImGui::Unindent(8.f);
                ImGui::TreePop();
            }
        };

        bool bPursuitActive = (strcmp(Brain.GetCurrentActionName(), "Pursuit") == 0);
        bool bEvadeActive   = (strcmp(Brain.GetCurrentActionName(), "Evade")   == 0);
        bool bWanderActive  = (strcmp(Brain.GetCurrentActionName(), "Wander")  == 0);

        auto ToVec = [](auto& Src) {
            std::vector<std::pair<std::string,float>> Out;
            for (auto& D : Src) Out.emplace_back(D.Name, D.Score);
            return Out;
        };

        auto PursuitScores = PursuitAction->GetConsiderationScores();
        auto EvadeScores   = EvadeAction->GetConsiderationScores();
        auto WanderScores  = WanderAction->GetConsiderationScores();

        DrawAction("Pursuit", PursuitAction->Score(), bPursuitActive, ToVec(PursuitScores));
        DrawAction("Evade",   EvadeAction->Score(),   bEvadeActive,   ToVec(EvadeScores));
        DrawAction("Wander",  WanderAction->Score(),  bWanderActive,  ToVec(WanderScores));
    }

    ImGui::End();
}