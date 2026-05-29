#pragma once

#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "Movement/SteeringBehaviors/SteeringAgent.h"
#include <functional>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Curve helpers
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Consideration — one input mapped through a curve to [0,1]
// T is the action's own context struct
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Base action (type-erased so the brain can hold a list)
// ---------------------------------------------------------------------------
class IUtilityAction
{
public:
    std::string Name;
    virtual ~IUtilityAction() = default;

    virtual float Score()                              const = 0;
    virtual void  Execute(ASteeringAgent&, float Dt)         = 0;
    virtual void  OnEnter(ASteeringAgent&)                   = 0;
    virtual void  OnExit (ASteeringAgent&)                   = 0;
};

// Helper: multiply all consideration scores + compensation factor
template<typename TContext>
static float CalcScore(const std::vector<TConsideration<TContext>>& Considerations,
                       const TContext& Ctx)
{
    if (Considerations.empty()) return 0.f;
    float total = 1.f;
    for (auto& c : Considerations)
        total *= c.Evaluate(Ctx);
    // Compensation: stops score collapsing with many considerations
    float mod = 1.f - (1.f / static_cast<float>(Considerations.size()));
    return FMath::Clamp(total + total * mod * (1.f - total), 0.f, 1.f);
}

// ---------------------------------------------------------------------------
// PURSUIT ACTION
// Context: how far the target is (normalized). 
//   Close  → high score (we can tag them)
//   Far    → low score  (pointless to chase)
// ---------------------------------------------------------------------------
struct FPursuitContext
{
    float NormalizedProximityToPlayer = 0.f;  // 0=far, 1=touching
    bool  bIsIt                      = false; // is this agent currently "it"?
};

class UAPursuitAction : public IUtilityAction
{
public:
    FPursuitContext                          Context;
    std::vector<TConsideration<FPursuitContext>> Considerations;

    UAPursuitAction()
    {
        Name = "Pursuit";

        // Hard gate: only pursue when "it"
        Considerations.push_back({
            "IsIt",
            [](const FPursuitContext& c){ return c.bIsIt ? 1.f : 0.f; },
            ECurveType::Linear
        });
        // High score when target is CLOSE — inverse: distance 0→score 1, distance 1→score 0
        Considerations.push_back({
            "TargetClose",
            [](const FPursuitContext& c){ return c.NormalizedProximityToPlayer; },
            ECurveType::SineCurve
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override
    {
        Agent.SetSteeringBehavior(&m_Pursuit);
    }
    void OnExit(ASteeringAgent& Agent) override {}

    void Execute(ASteeringAgent&, float) override
    {
        // SteeringAgent::Tick drives movement automatically
    }

    void SetTarget(const FTargetData& T) { m_Pursuit.SetTarget(T); }
    
    struct FConsiderationDebug { std::string Name; float Score; };

    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations)
            Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Pursuit m_Pursuit;
};

// ---------------------------------------------------------------------------
// EVADE ACTION
// Context: how close the "it" agent (seeker) is.
//   Seeker close + we're NOT it → high evade score
// ---------------------------------------------------------------------------
struct FEvadeContext
{
    float NormalizedProximityToSeeker = 0.f;  // 0=far, 1=touching
    bool  bIsIt                      = false;
};

class UAEvadeAction : public IUtilityAction
{
public:
    FEvadeContext                          Context;
    std::vector<TConsideration<FEvadeContext>> Considerations;

    UAEvadeAction()
    {
        Name = "Evade";

        // Only evade when we're NOT it
        Considerations.push_back({
            "NotIsIt",
            [](const FEvadeContext& c){ return c.bIsIt ? 0.f : 1.f; },
            ECurveType::Linear
        });
        // Score spikes when seeker is dangerously close (smooth so it feels urgent)
        Considerations.push_back({
            "SeekerProximity",
            [](const FEvadeContext& c){ return c.NormalizedProximityToSeeker; },
            ECurveType::Exponential
        });
    }

    float Score() const override { return CalcScore(Considerations, Context); }

    void OnEnter(ASteeringAgent& Agent) override
    {
        Agent.SetSteeringBehavior(&m_Evade);
    }
    void OnExit(ASteeringAgent& Agent) override {}
    void Execute(ASteeringAgent&, float) override {}

    void SetTarget(const FTargetData& T) { m_Evade.SetTarget(T); }
    
    struct FConsiderationDebug { std::string Name; float Score; };

    std::vector<FConsiderationDebug> GetConsiderationScores() const
    {
        std::vector<FConsiderationDebug> Out;
        for (auto& C : Considerations)
            Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Evade m_Evade;
};

// ---------------------------------------------------------------------------
// WANDER ACTION
// Context: how far the nearest threat is.
//   Seeker far + we're NOT it → wander is fine
//   Seeker close → wander score drops (evade should win)
// ---------------------------------------------------------------------------
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

        // High score when no one is around
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
        for (auto& C : Considerations)
            Out.push_back({ C.Name, C.Evaluate(Context) });
        return Out;
    }

private:
    Wander m_Wander;
};