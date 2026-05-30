#pragma once

#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "Movement/SteeringBehaviors/SteeringAgent.h"
#include <functional>
#include <vector>
#include <string>

// ===========================================================================
// Curve helpers
// ===========================================================================
enum class ECurveType : uint8_t
{
    Linear,
    InverseLinear,
    SmoothStep,
    SineCurve,
    Cosine,
    Exponential,
    Logarithmic,
};

inline const char* CurveTypeName(ECurveType T)
{
    switch (T) {
    case ECurveType::Linear:        return "Linear";
    case ECurveType::InverseLinear: return "InverseLinear";
    case ECurveType::SmoothStep:    return "SmoothStep";
    case ECurveType::SineCurve:     return "SineCurve";
    case ECurveType::Cosine:        return "Cosine";
    case ECurveType::Exponential:   return "Exponential";
    case ECurveType::Logarithmic:   return "Logarithmic";
    default:                        return "Unknown";
    }
}

inline float EvaluateCurve(ECurveType T, float x)
{
    float t = FMath::Clamp(x, 0.f, 1.f);
    switch (T) {
    case ECurveType::Linear:        return t;
    case ECurveType::InverseLinear: return 1.f - t;
    case ECurveType::SmoothStep:    return t * t * (3.f - 2.f * t);
    case ECurveType::SineCurve:     return (1.f - FMath::Cos(t * PI)) * 0.5f;
    case ECurveType::Cosine:        return FMath::Sin(t * PI * 0.5f);
    case ECurveType::Exponential:   return FMath::Pow(t, 2.f);
    case ECurveType::Logarithmic:   return FMath::Clamp(FMath::Loge(1.f + t * 9.f) / FMath::Loge(10.f), 0.f, 1.f);
    default:                        return t;
    }
}

// ===========================================================================
// Consideration — one input mapped through a curve to [0,1]
// ===========================================================================
template<typename TContext>
struct TConsideration
{
    std::string                           Name;
    std::function<float(const TContext&)> GetRawValue;
    ECurveType                            CurveType = ECurveType::Linear;

    float Evaluate(const TContext& Ctx) const
    {
        return EvaluateCurve(CurveType, GetRawValue(Ctx));
    }
};

// ===========================================================================
// Base action (type-erased so the brain can hold a list)
// ===========================================================================
class IUtilityAction
{
public:
    std::string Name;
    virtual ~IUtilityAction() = default;

    virtual float Score()                          const = 0;
    virtual void  Execute(ASteeringAgent&, float)        = 0;
    virtual void  OnEnter(ASteeringAgent&)               = 0;
    virtual void  OnExit (ASteeringAgent&)               = 0;
};

// Multiply all consideration scores + compensation factor to prevent
// score collapse when many considerations are active simultaneously
template<typename TContext>
static float CalcScore(const std::vector<TConsideration<TContext>>& Considerations,
                       const TContext& Ctx)
{
    if (Considerations.empty()) return 0.f;
    float total = 1.f;
    for (auto& c : Considerations)
        total *= c.Evaluate(Ctx);
    float mod = 1.f - (1.f / static_cast<float>(Considerations.size()));
    return FMath::Clamp(total + total * mod * (1.f - total), 0.f, 1.f);
}

// ===========================================================================
// PURSUIT ACTION  (UtilityTheory level)
// Context: how far the target is.
//   Close + agent is "it" → high score
// ===========================================================================
struct FPursuitContext
{
    float NormalizedProximityToPlayer = 0.f;
    bool  bIsIt                       = false;
};

class UAPursuitAction : public IUtilityAction
{
public:
    FPursuitContext                               Context;
    std::vector<TConsideration<FPursuitContext>>  Considerations;

    UAPursuitAction()
    {
        Name = "Pursuit";

        Considerations.push_back({
            "IsIt",
            [](const FPursuitContext& c){ return c.bIsIt ? 1.f : 0.f; },
            ECurveType::Linear
        });
        Considerations.push_back({
            "TargetClose",
            [](const FPursuitContext& c){ return c.NormalizedProximityToPlayer; },
            ECurveType::SineCurve
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override { Agent.SetSteeringBehavior(&m_Pursuit); }
    void OnExit (ASteeringAgent& Agent) override {}
    void Execute(ASteeringAgent&, float) override {}

    void SetTarget(const FTargetData& T) { m_Pursuit.SetTarget(T); }

    struct FConsiderationDebug { std::string Name; float Score; };
    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations) Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Pursuit m_Pursuit;
};

// ===========================================================================
// EVADE ACTION  (UtilityTheory level)
// Context: how close the seeker is.
//   Seeker close + NOT "it" → high score
// ===========================================================================
struct FEvadeContext
{
    float NormalizedProximityToSeeker = 0.f;
    bool  bIsIt                       = false;
};

class UAEvadeAction : public IUtilityAction
{
public:
    FEvadeContext                               Context;
    std::vector<TConsideration<FEvadeContext>>  Considerations;

    UAEvadeAction()
    {
        Name = "Evade";

        Considerations.push_back({
            "NotIsIt",
            [](const FEvadeContext& c){ return c.bIsIt ? 0.f : 1.f; },
            ECurveType::Linear
        });
        Considerations.push_back({
            "SeekerProximity",
            [](const FEvadeContext& c){ return c.NormalizedProximityToSeeker; },
            ECurveType::Exponential
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override { Agent.SetSteeringBehavior(&m_Evade); }
    void OnExit (ASteeringAgent& Agent) override {}
    void Execute(ASteeringAgent&, float) override {}

    void SetTarget(const FTargetData& T) { m_Evade.SetTarget(T); }

    struct FConsiderationDebug { std::string Name; float Score; };
    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations) Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Evade m_Evade;
};

// ===========================================================================
// WANDER ACTION  (UtilityTheory level)
// Context: how far the nearest threat is.
//   Threat far → wander scores high
// ===========================================================================
struct FWanderContext
{
    float NormalizedDistanceToPlayer = 1.f;
};

class UAWanderAction : public IUtilityAction
{
public:
    FWanderContext                               Context;
    std::vector<TConsideration<FWanderContext>>  Considerations;

    UAWanderAction()
    {
        Name = "Wander";

        Considerations.push_back({
            "SeekerFar",
            [](const FWanderContext& c){ return c.NormalizedDistanceToPlayer; },
            ECurveType::SmoothStep
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override { Agent.SetSteeringBehavior(&m_Wander); }
    void OnExit (ASteeringAgent& Agent) override {}
    void Execute(ASteeringAgent&, float) override {}

    struct FConsiderationDebug { std::string Name; float Score; };
    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations) Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Wander m_Wander;
};

// ===========================================================================
// Shared freeze tag context
// Passed to all three freeze tag actions so they reason from the same data.
//
//   NormalizedProximityToPrey    — 0=far, 1=touching (nearest unfrozen prey)
//   NormalizedProximityToThreat  — 0=far, 1=touching (nearest unfrozen threat)
//   DangerMeter                  — 0=safe, 1=critical
//                                  rises with frozen teammates,
//                                  falls when prey team is nearly fully frozen
//   FrozenTeammateNearby         — 0=no frozen mate in rescue range, 1=adjacent
// ===========================================================================
struct FFreezeTagContext
{
    float NormalizedProximityToPrey   = 0.f;
    float NormalizedProximityToThreat = 0.f;
    float DangerMeter                 = 0.f;
    float FrozenTeammateNearby        = 0.f;
};

// ===========================================================================
// FT_PursuitAction
// Pursue the nearest unfrozen prey.
//   High score when prey is close AND danger is low (team is winning).
//   Collapses when danger rises — reckless chasing when teammates are frozen
//   and a threat is active is punished.
//
// NOTE: LowDanger raw value is already (1 - DangerMeter) so Linear is correct.
//       Using InverseLinear here would double-invert and make it rise with danger.
// ===========================================================================
class FT_PursuitAction : public IUtilityAction
{
public:
    FFreezeTagContext                              Context;
    std::vector<TConsideration<FFreezeTagContext>> Considerations;

    // bCustom = true  → all curves start as Linear (player tunes them)
    // bCustom = false → fitted curves for the fixed utility team
    explicit FT_PursuitAction(bool bCustom = false)
    {
        Name = "Pursuit";

        // High when prey is close
        Considerations.push_back({
            "PreyClose",
            [](const FFreezeTagContext& c){ return c.NormalizedProximityToPrey; },
            bCustom ? ECurveType::Linear : ECurveType::SineCurve
        });
        // High when danger is LOW — raw value already inverted, Linear is correct
        Considerations.push_back({
            "LowDanger",
            [](const FFreezeTagContext& c){ return 1.f - c.DangerMeter; },
            ECurveType::Linear
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override { Agent.SetSteeringBehavior(&m_Pursuit); }
    void OnExit (ASteeringAgent& Agent) override {}
    void Execute(ASteeringAgent&, float) override {}

    void SetTarget(const FTargetData& T) { m_Pursuit.SetTarget(T); }

    struct FConsiderationDebug { std::string Name; float Score; };
    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations) Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Pursuit m_Pursuit;
};

// ===========================================================================
// FT_EvadeAction
// Flee from the nearest unfrozen threat.
//   Score spikes when a threat is close AND danger is already high —
//   the combination of personal threat + team struggle makes evading urgent.
// ===========================================================================
class FT_EvadeAction : public IUtilityAction
{
public:
    FFreezeTagContext                              Context;
    std::vector<TConsideration<FFreezeTagContext>> Considerations;

    explicit FT_EvadeAction(bool bCustom = false)
    {
        Name = "Evade";

        // Evade purely based on how close the threat is — no danger gate needed
        Considerations.push_back({
            "ThreatClose",
            [](const FFreezeTagContext& c){ return c.NormalizedProximityToThreat; },
            bCustom ? ECurveType::Linear : ECurveType::SmoothStep
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override { Agent.SetSteeringBehavior(&m_Evade); }
    void OnExit (ASteeringAgent& Agent) override {}
    void Execute(ASteeringAgent&, float) override {}

    void SetTarget(const FTargetData& T) { m_Evade.SetTarget(T); }

    struct FConsiderationDebug { std::string Name; float Score; };
    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations) Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Evade m_Evade;
};

// ===========================================================================
// FT_RescueAction
// Seek toward the nearest frozen teammate to unfreeze them.
//   Only meaningful when danger is high and a frozen teammate is nearby.
//   Evade still wins if a threat is right on top of the agent because
//   ThreatClose spikes harder than HighDanger alone.
// ===========================================================================
class FT_RescueAction : public IUtilityAction
{
public:
    FFreezeTagContext                              Context;
    std::vector<TConsideration<FFreezeTagContext>> Considerations;

    explicit FT_RescueAction(bool bCustom = false)
    {
        Name = "Rescue";

        // Primary driver — spikes hard when a frozen mate is very close
        Considerations.push_back({
            "FrozenMateNearby",
            [](const FFreezeTagContext& c){ return c.FrozenTeammateNearby; },
            bCustom ? ECurveType::Linear : ECurveType::SineCurve
        });
        // Small nudge — more frozen teammates = slightly more willing to rescue
        // Kept small (Exponential keeps it low until danger is really high)
        // so it only tips the scale when passing nearby, not overrides Evade from afar
        Considerations.push_back({
            "HighDanger",
            [](const FFreezeTagContext& c){ return 0.4f + 0.6f * c.DangerMeter; },
            bCustom ? ECurveType::Linear : ECurveType::Exponential
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override { Agent.SetSteeringBehavior(&m_Seek); }
    void OnExit (ASteeringAgent& Agent) override {}
    void Execute(ASteeringAgent&, float) override {}

    void SetTarget(const FTargetData& T) { m_Seek.SetTarget(T); }

    struct FConsiderationDebug { std::string Name; float Score; };
    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations) Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Seek m_Seek;
};