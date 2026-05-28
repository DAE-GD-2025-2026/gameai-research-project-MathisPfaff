#pragma once

#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "Movement/SteeringBehaviors/SteeringAgent.h"
#include <functional>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Curve helpers — plain lambdas, use them inline when adding considerations
// ---------------------------------------------------------------------------
namespace UtilityCurves
{
    // score rises as x rises  (0→0, 1→1)
    inline auto Linear()
    {
        return [](float x) { return FMath::Clamp(x, 0.f, 1.f); };
    }

    // score falls as x rises  (0→1, 1→0)  — good for "the closer, the higher"
    inline auto InverseLinear()
    {
        return [](float x) { return 1.f - FMath::Clamp(x, 0.f, 1.f); };
    }

    // smooth ease in/out
    inline auto SmoothStep()
    {
        return [](float x) {
            float t = FMath::Clamp(x, 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        };
    }

    // peaks at mid-range, low at extremes  — good for "medium distance"
    inline auto SineCurve()
    {
        return [](float x) {
            return FMath::Sin(FMath::Clamp(x, 0.f, 1.f) * PI);
        };
    }

    // rises sharply at high values
    inline auto Exponential(float exp = 2.f)
    {
        return [exp](float x) {
            return FMath::Pow(FMath::Clamp(x, 0.f, 1.f), exp);
        };
    }
}

// ---------------------------------------------------------------------------
// Consideration — one input mapped through a curve to [0,1]
// T is the action's own context struct
// ---------------------------------------------------------------------------
template<typename TContext>
struct TConsideration
{
    std::string                          Name;
    std::function<float(const TContext&)> GetRawValue;   // reads world data
    std::function<float(float)>           Curve;          // shapes the score

    float Evaluate(const TContext& Ctx) const
    {
        return FMath::Clamp(Curve(GetRawValue(Ctx)), 0.f, 1.f);
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
    float NormalizedDistanceToPlayer = 1.f;  // 0=touching, 1=max range
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
            UtilityCurves::Linear()
        });
        // High score when target is CLOSE — inverse: distance 0→score 1, distance 1→score 0
        Considerations.push_back({
            "TargetClose",
            [](const FPursuitContext& c){ return c.NormalizedDistanceToPlayer; },
            UtilityCurves::InverseLinear()
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
    float NormalizedDistanceToSeeker = 1.f; // 0=touching, 1=max range
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
            UtilityCurves::Linear()
        });
        // Score spikes when seeker is dangerously close (smooth so it feels urgent)
        Considerations.push_back({
            "SeekerProximity",
            [](const FEvadeContext& c){ return c.NormalizedDistanceToSeeker; },
            UtilityCurves::InverseLinear()   // swap to SmoothStep for a snappier feel
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
    float NormalizedDistanceToSeeker = 1.f;
};

class UAWanderAction : public IUtilityAction
{
public:
    FWanderContext                               Context;
    std::vector<TConsideration<FWanderContext>>  Considerations;

    UAWanderAction()
    {
        Name = "Wander";

        // High score when seeker is FAR — "no one around, go searching"
        Considerations.push_back({
            "SeekerFar",
            [](const FWanderContext& c){ return c.NormalizedDistanceToSeeker; },
            UtilityCurves::Linear()
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